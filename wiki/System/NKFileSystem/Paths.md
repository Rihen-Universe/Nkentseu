# Les chemins de fichiers

> Couche **System** · NKFileSystem · Manipuler un **chemin** de système de fichiers — fichier ou
> répertoire — sans `std::filesystem` : la classe `NkPath`, ses jointures, sa décomposition, ses
> conversions et ses utilitaires statiques.

Dès qu'un programme touche au disque — charger une texture, écrire une sauvegarde, lister un dossier
d'assets, retrouver le répertoire de l'exécutable — il manipule des **chemins**. Et un chemin, c'est
trompeusement piégeux : Windows écrit `C:\Jeux\save.dat` avec des antislashs, Unix écrit
`/home/jeux/save.dat` avec des slashs, certaines API veulent l'un, d'autres l'autre, et entre les
deux on passe son temps à découper « le répertoire », « le nom de fichier », « l'extension » à la
main avec des recherches de séparateur fragiles. `NkPath` règle tout cela : c'est un wrapper léger
autour d'une chaîne qui **normalise systématiquement** la représentation, sait se **décomposer**,
se **recomposer**, et se **reconvertir** au format natif uniquement quand il faut parler au système.

Le principe directeur tient en une phrase : **en interne, un `NkPath` est toujours stocké avec le
séparateur `'/'`**. Que vous le construisiez avec des antislashs Windows ou des slashs Unix, il
range `C:/Jeux/save.dat`. Vous travaillez donc partout avec `/`, sans vous soucier de la plateforme,
et vous n'appelez `ToNative()` qu'à la toute dernière seconde, à la frontière d'un appel système
Windows. Ce n'est **pas** `std::filesystem::path` (volontairement : le moteur vise l'embarqué et le
zéro-STL) ; ce n'est **pas** non plus une couche qui *touche* au disque (elle ne lit pas, n'écrit
pas, ne résout pas les `..`) — c'est de la pure manipulation de **texte de chemin**.

- **Namespace** : `nkentseu` (alias legacy `nkentseu::entseu::NkPath`)
- **Header** : `#include "NKFileSystem/NkPath.h"`

> **En résumé.** `NkPath` = un chemin stocké en interne avec `'/'`, normalisé à la construction, sans
> `std::filesystem` et sans I/O. On l'assemble, on le découpe, on le compare ; on ne reconvertit en
> natif (`ToNative()`) qu'au moment d'appeler le système.

---

## Construire et normaliser un chemin

On crée un `NkPath` à partir de rien (chemin vide), d'une C-string, d'une `NkString`, ou par copie
d'un autre chemin. Le point crucial : **tous les constructeurs (sauf le défaut) normalisent les
séparateurs** — les `'\\'` Windows deviennent des `'/'`. Vous pouvez donc coller une chaîne venue de
n'importe où, elle sera rangée proprement.

```cpp
NkPath a;                              // vide
NkPath b{ "C:\\Jeux\\save.dat" };      // stocké en interne "C:/Jeux/save.dat"
NkPath c{ NkString("assets/tex") };    // depuis NkString
NkPath d = b;                          // copie
```

Les trois opérateurs d'affectation (`= NkPath`, `= const char*`, `= NkString`) normalisent eux aussi
et renvoient `*this` pour le chaînage. Il n'y a **pas** de constructeur ni d'opérateur de *move*
déclaré : un `NkPath` n'est qu'une `NkString` enveloppée, la copie est une copie de chaîne.

> **En résumé.** Construisez librement avec des `\` ou des `/` : la normalisation rend l'interne
> toujours en `/`. Affectations chaînables, pas de sémantique de déplacement dédiée.

---

## Assembler : `Append` (mutant) contre `operator/` (copie)

Composer un chemin à partir de morceaux est l'opération la plus courante. `NkPath` en offre **deux
familles**, et la distinction est essentielle. `Append(...)` **modifie l'objet en place** et renvoie
`*this` (donc chaînable), en insérant le séparateur manquant si besoin. `operator/(...)` est
**`const`** : il ne touche pas à l'instance et **renvoie un nouveau** `NkPath` — exactement comme on
écrirait une division, dans le style POSIX.

```cpp
NkPath base{ "C:/Jeux" };

// Non destructif : 'base' reste "C:/Jeux"
NkPath cfg = base / "profils" / "rihen" / "config.ini";

// Mutant : 'base' devient "C:/Jeux/data/levels"
base.Append("data").Append("levels");
```

C'est **le** piège à intégrer : `p / "x"` ne change pas `p` (il faut récupérer le résultat),
`p.Append("x")` change `p`. Comme aucune méthode n'est `nodiscard`, oublier de récupérer le retour
de `operator/` ne déclenche **aucun avertissement** du compilateur.

> **En résumé.** `Append` = en place et chaînable ; `operator/` = copie, style `base / "a" / "b"`.
> Les deux insèrent le séparateur tout seuls. `operator/` n'altère jamais l'original.

---

## Décomposer et interroger un chemin

Une fois le chemin assemblé, on veut souvent en extraire des morceaux : le répertoire parent, le nom
de fichier, l'extension. Toutes ces méthodes sont **`const`** et renvoient une `NkString` (une copie
indépendante), sans jamais modifier le `NkPath`. `GetDirectory()` donne le parent, `GetFileName()` le
nom complet avec extension, `GetFileNameWithoutExtension()` le tronc, `GetExtension()` l'extension
**point compris** (`".png"`), et `GetRoot()` la racine (`"C:/"` ou `"/"`, vide si relatif).

```cpp
NkPath p{ "C:/Jeux/assets/hero.png" };
p.GetDirectory();                 // "C:/Jeux/assets"
p.GetFileName();                  // "hero.png"
p.GetFileNameWithoutExtension();  // "hero"
p.GetExtension();                 // ".png"  (le point est inclus)
p.GetRoot();                      // "C:/"
```

À côté, des **prédicats** booléens : `IsAbsolute()` / `IsRelative()` (commence-t-il par une racine ?),
`HasExtension()` et `HasFileName()`. Un détail de comportement documenté à connaître :
`".gitignore"` est considéré comme **ayant une extension** (`HasExtension() == true`).

> **En résumé.** Les `Get*` de décomposition sont des lectures `const` qui renvoient des copies ;
> `GetExtension()` inclut le point. Les prédicats `Is*`/`Has*` répondent par `bool`. Rien ici ne
> modifie le chemin.

---

## Transformer : remplacer, retirer, remonter

Pour fabriquer un chemin dérivé d'un autre — changer l'extension d'un fichier compilé, renommer en
gardant le dossier, remonter au parent — il y a un jeu de transformations. Attention, elles ne sont
**pas** toutes du même genre. `ReplaceExtension`, `ReplaceFileName` et `RemoveFileName` sont
**mutantes** (elles modifient `mPath` et renvoient `*this`), tandis que `GetParent()` est **non
destructif** (il renvoie un nouvel objet sans toucher l'original).

```cpp
NkPath src{ "C:/Jeux/shaders/main.vert" };

NkPath obj = src;
obj.ReplaceExtension("spv");          // "C:/Jeux/shaders/main.spv"  (le point est ajouté seul)

NkPath parent = src.GetParent();      // "C:/Jeux/shaders"  ('src' inchangé)

src.RemoveFileName();                 // 'src' devient "C:/Jeux/shaders/"  (in place !)
```

Ne **jamais** confondre `RemoveFileName()` (effet de bord sur l'objet) et `GetParent()` (résultat à
récupérer). `ReplaceExtension` ajoute le `'.'` automatiquement si vous ne le mettez pas ;
`ReplaceFileName` change aussi l'extension si le nouveau nom en contient une.

> **En résumé.** `Replace*` et `RemoveFileName` **modifient** l'objet ; `GetParent` **copie**.
> `ReplaceExtension` met le point tout seul. Le couple piège : `RemoveFileName()` (in place) ≠
> `GetParent()` (nouveau).

---

## Convertir pour le système

En interne tout est en `'/'`, mais le monde extérieur ne veut pas forcément ça. Trois sorties :
`CStr()` renvoie un `const char*` **vers la chaîne interne** (valide tant que le `NkPath` vit, lecture
seule) ; `ToString()` renvoie une **copie** `NkString` (toujours en `'/'`) ; `ToNative()` renvoie le
chemin au **format de la plateforme** — sur Windows il reconvertit `'/'`→`'\\'`, sur Unix il laisse
tel quel.

```cpp
NkPath p{ "C:/Jeux/save.dat" };
const char* raw = p.CStr();     // "C:/Jeux/save.dat"  (durée de vie = p)
NkString s      = p.ToString(); // "C:/Jeux/save.dat"  (copie)
NkString native = p.ToNative(); // Windows: "C:\\Jeux\\save.dat" ; Unix: inchangé
```

L'idiome recommandé : garder `'/'` partout dans votre code, et n'appeler `ToNative()` qu'au moment
précis d'invoquer une API système Windows (sur Unix, `ToString()` suffit déjà).

> **En résumé.** `CStr()` = pointeur interne, lecture seule, durée de vie liée à l'objet ;
> `ToString()` = copie en `/` ; `ToNative()` = format plateforme. Reconvertir en natif seulement à la
> frontière système.

---

## Comparer

`operator==` et `operator!=` comparent les chemins **caractère par caractère** sur la représentation
interne. C'est une comparaison **littérale** : pas de résolution de `..`/`.`, pas de canonicalisation.
Deux chemins logiquement équivalents mais écrits différemment (`a/b` vs `a/./b`) ne sont donc **pas**
égaux.

> **En résumé.** `==`/`!=` sont **textuels**. Si vous avez besoin d'égalité logique, normalisez les
> chemins vous-même avant de comparer.

---

## Aperçu de l'API

La liste de **tous** les éléments publics de `nkentseu::NkPath`. Aucune méthode n'est annotée
`noexcept` / `nodiscard` / `constexpr` ; les complexités entre crochets sont **déduites** du contrat
(manipulation d'une chaîne de longueur n), pas inscrites dans le code.

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkPath()` `[O(1)]` | Chemin vide. |
| Construction | `NkPath(const char*)` `[O(n)]` | Depuis C-string, normalise `\`→`/`. |
| Construction | `NkPath(const NkString&)` `[O(n)]` | Depuis `NkString`, normalise. |
| Construction | `NkPath(const NkPath&)` `[O(n)]` | Copie. |
| Affectation | `operator=` (NkPath / const char* / NkString) | Affecte et normalise ; renvoie `*this`. |
| Jointure (mutant) | `Append(const char*)` `[O(n+m)]` | Ajoute un composant en place ; renvoie `*this`. |
| Jointure (mutant) | `Append(const NkPath&)` · `Append(const NkString&)` | Idem, autres types. |
| Jointure (copie) | `operator/(NkString)` · `operator/(const char*)` · `operator/(NkPath)` `const` | Jointure non destructive ; nouvel objet. |
| Décomposition | `GetDirectory()` `const` `[O(n)]` | Répertoire parent (`""` si pas de séparateur). |
| Décomposition | `GetFileName()` `const` `[O(n)]` | Nom + extension. |
| Décomposition | `GetFileNameWithoutExtension()` `const` | Nom sans extension. |
| Décomposition | `GetExtension()` `const` | Extension **point inclus** (`""` si aucune). |
| Décomposition | `GetRoot()` `const` | Racine (`"C:/"`/`"/"`, `""` si relatif). |
| Prédicats | `IsAbsolute()` · `IsRelative()` `const` | Absolu (commence par une racine) / relatif. |
| Prédicats | `HasExtension()` · `HasFileName()` `const` | A une extension / un nom de fichier. |
| Modification (mutant) | `ReplaceExtension(const char*)` · `(NkString)` | Remplace l'extension (`.` auto) ; `*this`. |
| Modification (mutant) | `ReplaceFileName(const char*)` · `(NkString)` | Remplace le nom (garde le dossier) ; `*this`. |
| Modification (mutant) | `RemoveFileName()` | Ne garde que le répertoire ; `*this`. |
| Modification (copie) | `GetParent()` `const` | Répertoire parent, **nouvel objet**. |
| Conversion | `CStr()` `const` | `const char*` interne (durée de vie = objet, lecture seule). |
| Conversion | `ToString()` `const` | Copie `NkString` (séparateurs `/`). |
| Conversion | `ToNative()` `const` | Format natif (Windows `/`→`\` ; Unix inchangé). |
| Comparaison | `operator==` · `operator!=` `const` | Égalité **textuelle** (pas de canonicalisation). |
| Statique | `GetCurrentDirectory()` | Répertoire de travail courant (cwd absolu). |
| Statique | `GetNkCurrentDirectory()` | Variante du répertoire courant (non documentée). |
| Statique | `GetExecutableDirectory()` | Répertoire contenant l'exécutable. |
| Statique | `GetTempDirectory()` | Répertoire temporaire système. |
| Statique | `Combine(const char*, const char*)` · `(NkString, NkString)` | Combine deux chemins (args null/vides tolérés). |
| Statique | `IsValidPath(const char*)` · `(NkString)` | Valide l'absence de caractères interdits. |
| Alias | `nkentseu::entseu::NkPath` | Alias de type legacy (`using`). |

---

## Référence complète

Chaque élément repris en détail, avec comportement et usages dans les différents domaines du moteur.
Les éléments triviaux (construction, affectation, comparaison) sont brefs ; les opérations
structurantes (jointure, décomposition, conversion, statiques) sont traitées à fond.

### Construction et affectation

Quatre constructeurs (vide, `const char*`, `NkString`, copie) et trois affectations. Tous sauf le
constructeur par défaut appellent `NormalizeSeparators()` qui remplace in-place les `'\\'` par `'/'`
(`O(n)`). L'objet n'enveloppe qu'une `NkString` (`mPath`) : la copie est une copie de chaîne, le
RAII est trivial, et il n'y a **pas** de *move* déclaré (un transfert se fait donc par copie de la
chaîne sous-jacente, gérée par `NkString`/les allocateurs NKContainers — jamais de `new`/`delete`
brut). En pratique :

- **IO / outils** — coller des chemins venus de la ligne de commande, d'un fichier de config, d'un
  glisser-déposer : peu importe la casse des séparateurs en entrée, l'interne est uniforme.
- **Éditeur** — stocker dans un projet (`.nkproj`) des chemins d'assets relatifs en `/`, portables
  d'une machine Windows à une machine Unix sans retraitement.

### `Append` — jointure en place

`Append` ajoute un composant à la fin du chemin courant, en insérant le séparateur `'/'` si l'un des
deux côtés n'en porte pas, puis renvoie `*this` (chaînable). Coût `O(n+m)` (longueur courante +
ajout). Comme c'est **mutant**, on l'emploie quand on construit progressivement un chemin qu'on
possède déjà :

- **Rendu / GPU** — bâtir le chemin d'un shader compilé étape par étape (`shaderDir.Append(name).Append("spv")` après extension) dans un chargeur de pipeline.
- **Audio** — assembler le chemin d'une banque de sons à partir d'un dossier racine et d'un nom de
  bus, en accumulant les segments dans une boucle.
- **IO / réseau** — composer une arborescence de cache (`cacheRoot.Append(hash)`) où l'objet racine
  est réutilisé et étendu à chaque entrée.

### `operator/` — jointure non destructive

Trois surcharges `const` (`NkString`, `const char*`, `NkPath`) qui **renvoient un nouveau** chemin
sans toucher l'original. C'est l'idiome lisible `base / "a" / "b" / "c.ext"`, calqué sur la syntaxe
POSIX et sur `std::filesystem`. À privilégier dès qu'on dérive un chemin d'une base **qu'on veut
conserver** :

- **Éditeur / outils** — un dossier de projet `root` réutilisé pour fabriquer `root / "scenes" / name`, `root / "assets" / tex`, `root / "config.ini"` sans jamais altérer `root`.
- **Gameplay** — résoudre le chemin d'une sauvegarde `saveDir / slotName` à la volée à chaque slot,
  l'original restant le dossier de sauvegardes.
- **UI / 2D** — pointer une icône `themeDir / iconName` dans un thème, en gardant `themeDir`
  réutilisable pour toutes les icônes.

Piège transverse : `operator/` n'étant pas `nodiscard`, `p / "x";` seul (sans affectation) compile
sans broncher et ne fait **rien** d'utile.

### `GetDirectory`, `GetFileName`, `GetFileNameWithoutExtension`, `GetExtension`, `GetRoot`

Cinq lectures `const` renvoyant une `NkString` (copie indépendante), toutes `O(n)` (scan jusqu'au
dernier séparateur ou au dernier point). Comportements de bord à mémoriser : `GetDirectory()` rend
`""` s'il n'y a pas de séparateur (fichier seul) ; `GetFileName()` rend le chemin complet dans ce
même cas ; `GetExtension()` **inclut le point** (`".txt"`) et rend `""` s'il n'y en a pas ;
`GetRoot()` rend `""` pour un chemin relatif. Usages :

- **Rendu / GPU** — `GetExtension()` aiguille le chargeur d'images (`.png` / `.hdr` / `.dds`) vers le
  bon codec NKImage ou le bon format GPU.
- **Outils / build** — `GetFileNameWithoutExtension()` donne le **tronc** d'un asset pour nommer la
  sortie compilée (`hero.fbx` → `hero.mesh`) ou la clé d'un cache.
- **Éditeur** — `GetDirectory()` localise le dossier d'un asset pour résoudre ses dépendances
  voisines (un matériau cherchant ses textures à côté du modèle).
- **IO** — `GetRoot()` détecte le volume d'un chemin absolu (choix d'un disque, montage réseau).

### `IsAbsolute`, `IsRelative`, `HasExtension`, `HasFileName`

Quatre prédicats `const` renvoyant `bool`. `IsAbsolute()` est vrai quand le chemin commence par une
racine (`C:/` ou `/`) ; `IsRelative()` est son inverse. `HasExtension()` reflète `GetExtension()` non
vide — avec la subtilité documentée que `".gitignore"` **a** une extension. `HasFileName()` reflète
`GetFileName()` non vide. Usages :

- **IO / réseau** — refuser ou résoudre un chemin relatif reçu d'une source externe avant de
  l'ouvrir (`IsAbsolute()` comme garde de sécurité minimale).
- **Éditeur / outils** — décider d'afficher un sélecteur de fichier (`HasFileName()`) ou un sélecteur
  de dossier ; filtrer une liste sur `HasExtension()`.
- **Build** — router un argument selon qu'il désigne un répertoire ou un fichier précis.

### `ReplaceExtension`, `ReplaceFileName`, `RemoveFileName` — transformations en place

Toutes **mutantes**, renvoient `*this`. `ReplaceExtension` change l'extension et **ajoute le `'.'`
automatiquement** si l'argument n'en contient pas. `ReplaceFileName` remplace le nom de fichier en
**conservant le répertoire**, et change aussi l'extension si le nouveau nom en porte une.
`RemoveFileName` ne garde que le répertoire. Usages :

- **Rendu / GPU** — `ReplaceExtension("spv")` ou `("dds")` pour passer d'un asset source à sa version
  compilée dans un pipeline de cuisson (cooking).
- **Outils / build** — `ReplaceFileName` pour produire un fichier voisin (`.meta`, `.log`) dans le
  même dossier ; `RemoveFileName` pour obtenir le répertoire de travail d'un fichier de sortie.
- **Animation / IO** — dériver le chemin d'un sidecar (un `.anim` à côté d'un `.fbx`) en réutilisant
  le même objet.

### `GetParent` — le parent, sans effet de bord

`GetParent()` est `const` et **renvoie un nouveau** `NkPath` égal au répertoire parent, sans modifier
l'instance. C'est le pendant non destructif de `RemoveFileName()`. À utiliser quand on a besoin du
dossier conteneur **tout en gardant** le chemin du fichier :

- **Éditeur** — afficher « ouvrir le dossier contenant » sans perdre la référence au fichier
  sélectionné.
- **IO** — créer l'arborescence parente (`GetParent()`) avant d'écrire un fichier, le chemin complet
  restant disponible pour l'écriture elle-même.

Rappel du piège : `GetParent()` produit un **résultat à récupérer** ; `RemoveFileName()` produit un
**effet**. Comme aucun n'est `nodiscard`, le compilateur ne vous rattrapera pas si vous appelez
`GetParent()` et jetez son retour par mégarde.

### `CStr`, `ToString`, `ToNative` — sortir du `NkPath`

`CStr()` rend un `const char*` pointant **directement** sur la chaîne interne normalisée : valide
seulement tant que le `NkPath` existe, en **lecture seule**, à ne jamais conserver au-delà de la vie
de l'objet ni modifier. `ToString()` rend une **copie** `NkString` indépendante (toujours en `'/'`).
`ToNative()` rend le chemin au format plateforme — `'/'`→`'\\'` sur Windows, inchangé sur Unix. La
règle d'or : tout garder en `'/'` et n'appeler `ToNative()` qu'à la **frontière** d'un appel système
Windows. Usages :

- **IO / système** — `ToNative()` juste avant `CreateFileW`/`fopen` côté Windows ; `ToString()` ou
  `CStr()` suffisent côté Unix.
- **GPU / chargeurs** — `CStr()` pour passer un chemin en lecture à une API C qui prend un `const
  char*`, sans allouer de copie (durée de vie maîtrisée le temps de l'appel).
- **Éditeur / sérialisation** — `ToString()` pour écrire le chemin **portable** (en `/`) dans un
  fichier projet, indépendant de la plateforme d'édition.

### `operator==`, `operator!=` — comparaison textuelle

Égalité/inégalité `const`, **caractère par caractère** sur la représentation interne. Aucune
canonicalisation : `a/b` ≠ `a/./b`, et un `..` n'est pas résolu. C'est rapide et prévisible, mais ce
n'est **pas** une égalité logique de chemins. Pour un dédoublonnage robuste (clés d'un cache d'assets,
détection de doublons dans une liste de fichiers), normalisez vous-même la forme avant de comparer.

- **Éditeur / outils** — comparer rapidement deux références d'asset déjà normalisées par le moteur.
- **IO** — détecter qu'un chemin n'a pas changé entre deux frames (rechargement à chaud) quand on
  contrôle la forme des deux côtés.

### Statiques d'environnement : `GetCurrentDirectory`, `GetExecutableDirectory`, `GetTempDirectory`, `GetNkCurrentDirectory`

Quatre fabriques statiques renvoyant un `NkPath`. `GetCurrentDirectory()` rend le **répertoire de
travail courant** du processus (cwd absolu), via `getcwd`/`_getcwd` selon la plateforme — c'est ici
que le `#undef GetCurrentDirectory` du header protège le nom de la méthode contre la macro `windows.h`
(sans quoi elle deviendrait `GetCurrentDirectoryA/W`). `GetExecutableDirectory()` rend le dossier
**contenant le binaire**, plus robuste que le cwd quand le programme est lancé depuis ailleurs
(raccourci, IDE, CI). `GetTempDirectory()` rend le **dossier temporaire système** (Windows :
`GetTempPathA`, sinon fallback `"C:/Temp"` ; Unix : `$TMPDIR`, sinon `"/tmp"`).
`GetNkCurrentDirectory()` est une **variante** du répertoire courant, **non documentée** dans le
header — son comportement exact (par rapport à `GetCurrentDirectory()`) est à vérifier dans le `.cpp`
avant tout usage. Usages :

- **IO / chargement d'assets** — `GetExecutableDirectory()` comme **ancre** pour résoudre le dossier
  d'assets relativement au binaire, indépendamment du cwd (le réflexe correct en production).
- **Outils / build** — `GetCurrentDirectory()` pour interpréter des chemins relatifs passés en
  arguments d'une commande.
- **IO / temporaire** — `GetTempDirectory()` pour écrire un fichier intermédiaire (cache de
  shaders compilés, export temporaire, dump de crash) puis le déplacer.

### Statiques utilitaires : `Combine`, `IsValidPath`

`Combine(path1, path2)` (surcharges `const char*` et `NkString`) **assemble deux chemins** en gérant
les arguments null ou vides — pratique quand l'un des deux peut manquer (préfixe optionnel + nom).
`IsValidPath(path)` **valide** l'absence de caractères interdits : il rejette `< > " | ? *` et tout
caractère de contrôle (code < 32) — et, d'après l'exemple de référence, la chaîne vide est elle aussi
rejetée. Usages :

- **Éditeur / UI** — `IsValidPath()` pour valider en direct un nom saisi par l'utilisateur (nouveau
  fichier, renommage) avant d'autoriser la validation.
- **IO / réseau** — `IsValidPath()` comme garde contre des noms forgés reçus d'une source externe.
- **Outils** — `Combine()` pour fusionner une racine configurable et un sous-chemin éventuellement
  absent, sans tester soi-même les cas vides.

### Alias `nkentseu::entseu::NkPath`

Un simple `using NkPath = ::nkentseu::NkPath;` placé dans le namespace imbriqué `entseu`, pour la
**compatibilité ascendante** d'anciens codes. Aucun symbole, aucun comportement supplémentaire : le
type est rigoureusement le même.

### Notes transverses

- **Normalisation systématique** — toute entrée `\` devient `/` en interne ; écrire des chemins en
  `/` partout est sûr, même sur Windows. Reconvertir avec `ToNative()` seulement à la frontière
  système Windows.
- **Mutant contre non-mutant** — `Append` / `Replace*` / `RemoveFileName` modifient et renvoient
  `*this` ; `operator/` et `GetParent` renvoient un nouvel objet sans toucher l'original.
- **Aucune annotation** — pas de `noexcept` / `nodiscard` / `constexpr` sur l'API publique : les
  retours (`operator/`, `GetParent`, prédicats…) peuvent être ignorés sans avertissement. D'où la
  vigilance sur `GetParent()` (résultat) vs `RemoveFileName()` (effet).
- **Threading** — aucune garantie de thread-safety : un `NkPath` n'est qu'un wrapper de `NkString`,
  le partage concurrent est à protéger côté appelant. Les statiques reposent sur l'état du processus
  et l'environnement (non garanties ré-entrantes).
- **Allocation** — toute la mémoire passe par `NkString` / `NkVector` (allocateurs NKContainers,
  zéro-STL) ; aucun `new`/`delete` brut exposé.

---

### Exemple récapitulatif

```cpp
#include "NKFileSystem/NkPath.h"
using namespace nkentseu;

// Ancrer les assets sur l'exécutable, pas sur le cwd.
NkPath assets = NkPath::GetExecutableDirectory() / "assets";

// Jointure non destructive : 'assets' reste intact.
NkPath tex = assets / "textures" / "hero.png";

tex.GetExtension();   // ".png"  → aiguille le codec
tex.GetFileName();    // "hero.png"
tex.IsAbsolute();     // true

// Dériver la version compilée (mutant) sans perdre la source.
NkPath compiled = tex;
compiled.ReplaceExtension("dds");          // ".../hero.dds"

// Créer le dossier parent avant d'écrire (parent = copie).
NkPath dir = compiled.GetParent();         // ".../textures"

// Valider un nom saisi, puis combiner avec un dossier temporaire.
if (NkPath::IsValidPath("dump.log")) {
    NkPath tmp = NkPath::Combine(NkPath::GetTempDirectory().ToString(), "dump.log");
    // ... reconvertir au format natif uniquement au moment de l'appel système :
    NkString native = tmp.ToNative();      // Windows: "...\\dump.log"
}
```

---

[← Index NKFileSystem](README.md) · [Récap NKFileSystem](../NKFileSystem.md) · [Couche System](../README.md)
