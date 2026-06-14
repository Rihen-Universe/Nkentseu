# Boîtes de dialogue natives & Launcher

> Couche **Runtime** · NKWindow · Demander un fichier, afficher un message, choisir une couleur via les
> dialogues **du système** (`NkDialogs`), et déléguer à l'OS l'ouverture d'une URL, d'un fichier ou
> d'un dossier (`NkLauncher`).

Tôt ou tard, une application a besoin de **parler au système d'exploitation plutôt qu'à sa propre
fenêtre** : « ouvre-moi ce fichier de scène », « préviens l'utilisateur que la sauvegarde a échoué »,
« montre le dossier des captures dans l'explorateur », « ouvre la doc dans le navigateur ». Ce ne sont
pas des éléments d'interface qu'on dessine soi-même — ce sont des **services natifs** que chaque OS
fournit déjà, avec son propre look, ses raccourcis et ses conventions. NKWindow expose ces services
sous deux petites classes purement statiques : `NkDialogs` (les boîtes de dialogue modales) et
`NkLauncher` (la délégation d'ouverture à l'application par défaut).

L'idée force est qu'on **ne réimplémente rien** : sur Windows on tombe sur les vraies boîtes natives,
sur Linux sur Zenity, sur macOS sur `osascript`, et sinon sur des stubs. L'appelant ne voit qu'une
API portable ; le moteur choisit le bon backend selon la plateforme. Deux classes, un struct de
résultat, aucun objet à instancier.

- **Namespace** : `nkentseu` (les deux headers)
- **Headers** : `#include "NKWindow/Core/NkDialogs.h"` · `#include "NKWindow/Core/NkLauncher.h"`

Ce ne sont **pas** des widgets de votre couche UI (cf. NKUI) : ce sont les dialogues du **système**,
modaux et synchrones, qui rendent la main une fois que l'utilisateur a répondu.

---

## Les boîtes de dialogue : `NkDialogs`

`NkDialogs` répond à un besoin précis : **demander quelque chose à l'utilisateur via une fenêtre que
le système connaît déjà**. Quatre opérations couvrent les cas courants — ouvrir un fichier, en
sauvegarder un, afficher un message, choisir une couleur. La classe **ne s'instancie pas** : elle n'a
que des méthodes `static` et aucun état interne, on l'appelle toujours via `NkDialogs::...`.

Toutes ces boîtes sont **modales et synchrones** : l'appel **bloque** jusqu'à ce que l'utilisateur
ferme le dialogue, exactement comme le veut le comportement natif d'un *open file* ou d'un *message
box*. Quand l'appel rend la main, on inspecte le résultat.

### Le résultat : `NkDialogResult`

Trois des quatre méthodes renvoient un `NkDialogResult`, un simple **agrégat POD** dont tous les
champs ont un initialisateur par défaut (pas de constructeur explicite à appeler) :

- `confirmed` (`bool`, défaut `false`) — `true` si l'utilisateur a **validé** le dialogue (et non
  annulé) ;
- `path` (`NkString`, défaut vide) — le chemin sélectionné, pour les dialogues de fichier ;
- `color` (`uint32`, défaut `0`) — la couleur choisie en RGBA, pour le color picker.

Le champ `path` est une `NkString` (conteneur maison) : le résultat **possède et copie** sa propre
chaîne, libérée par son propre destructeur. Il n'y a donc **rien à libérer à la main**, pas de
Create/Destroy. La règle d'or, immuable : **tester `confirmed` avant de lire `path` ou `color`** — si
l'utilisateur a annulé, ces champs ne veulent rien dire.

```cpp
NkDialogResult r = NkDialogs::OpenFileDialog("*.png;*.jpg", "Ouvrir une texture");
if (r.confirmed) {
    LoadTexture(r.path);   // sûr : l'utilisateur a validé
}
```

### Ouvrir et sauvegarder un fichier

`OpenFileDialog(filter, title)` ouvre le sélecteur d'**ouverture**. Le `filter` (défaut `"*.*"`)
restreint les types affichés sous la forme `"*.png;*.jpg"` et peut rester vide pour tout accepter ; le
`title` (défaut `"Open File"`) titre la fenêtre. Au retour, `confirmed` et `path` sont renseignés.

`SaveFileDialog(defaultExt, title)` ouvre le sélecteur de **sauvegarde**. Subtilité à retenir :
`defaultExt` est l'extension par défaut **sans le point** (`"png"`, pas `".png"`), vide par défaut ;
`title` vaut `"Save File"` par défaut. Même résultat : `confirmed` + `path`.

```cpp
NkDialogResult s = NkDialogs::SaveFileDialog("nkscene", "Enregistrer la scène");
if (s.confirmed)
    scene.SaveTo(s.path);
```

> **En résumé.** `OpenFileDialog` / `SaveFileDialog` rendent un `NkDialogResult` (`confirmed` +
> `path`). `filter` au format `"*.png;*.jpg"` (vide = tout), `defaultExt` **sans point**. Bloquants ;
> toujours tester `confirmed` avant `path`.

### Afficher un message

`OpenMessageBox(message, title, type)` affiche une boîte d'information. C'est la seule des quatre
méthodes qui **ne retourne rien** : elle informe, elle ne capture pas de choix Oui/Non. Le `type` est
un **`int` brut** (pas d'enum) : `0 = info`, `1 = warning`, `2 = error`, défaut `0`. On utilise donc
les littéraux 0/1/2.

```cpp
NkDialogs::OpenMessageBox("Le fichier est corrompu.", "Erreur", 2);  // 2 = error
```

### Choisir une couleur

`ColorPicker(initial)` ouvre le sélecteur de couleur natif. `initial` est la couleur de départ en
RGBA (défaut `0xFFFFFFFF`, blanc opaque). Le résultat porte `confirmed` et `color` :

```cpp
NkDialogResult c = NkDialogs::ColorPicker(0xFF0000FF);  // rouge opaque au départ
if (c.confirmed)
    brush.SetColor(c.color);
```

> **En résumé.** `OpenMessageBox` informe sans rien renvoyer (`type` = 0/1/2, littéraux bruts).
> `ColorPicker` rend `confirmed` + `color` (RGBA). Aucun appel n'est `noexcept` ; les paramètres
> texte passent par `const NkString&`.

---

## Le launcher : `NkLauncher`

`NkLauncher` répond à une question différente : **comment laisser l'OS ouvrir quelque chose avec
l'application par défaut ?** Une URL dans le navigateur, un PDF dans la visionneuse, un dossier dans
l'explorateur. Comme `NkDialogs`, c'est une classe **purement statique**, sans état, qu'on appelle via
`NkLauncher::...`. Mais deux différences d'API la distinguent nettement, et il faut les avoir en tête.

D'abord, ses trois méthodes prennent un **`const char*`** brut (et non `NkString`), et sont toutes
**`noexcept`**. Ensuite, leur **valeur de retour** a une sémantique précise : `true` signifie que
**l'invocation système a été déclenchée** avec succès — pas que l'utilisateur a effectivement vu la
page ou le fichier. On lance l'action et on rend la main ; on **n'attend pas** l'aboutissement côté
navigateur ou application. `false` signale une erreur (URL nulle, plateforme non supportée, échec
JNI…). Le booléen est significatif : **testez-le** même si la méthode n'est pas `nodiscard`.

```cpp
if (!NkLauncher::OpenURL("https://docs.nkentseu.dev"))
    NkDialogs::OpenMessageBox("Impossible d'ouvrir le navigateur.", "Erreur", 2);
```

Sous le capot, la stratégie diffère par plateforme (Windows `ShellExecuteA`, Linux
`system("xdg-open …")`, macOS `system("open …")`, Android Intent ACTION_VIEW via JNI, iOS
`UIApplication openURL`, Emscripten `window.open`) — mais l'appelant ne voit qu'une API uniforme.

### Ouvrir une URL, un fichier, un dossier

Les trois méthodes sont symétriques :

- `OpenURL(url)` — ouvre une URL dans le navigateur par défaut (`https://…` ou tout protocole que l'OS
  sait gérer).
- `OpenFile(filePath)` — ouvre un fichier dans l'application associée à son extension (un `.pdf` part
  dans la visionneuse PDF, un `.png` dans la visionneuse d'images).
- `OpenFolder(folderPath)` — ouvre un dossier dans l'explorateur système (Explorer, Finder, Nautilus…).

```cpp
NkLauncher::OpenFile("Captures/screenshot.png");   // visionneuse d'images
NkLauncher::OpenFolder("Captures");                // explorateur de fichiers
NkLauncher::OpenURL("https://github.com/nkentseu"); // navigateur
```

> **En résumé.** `OpenURL` / `OpenFile` / `OpenFolder` délèguent à l'application par défaut de l'OS.
> Arguments en `const char*`, toutes `noexcept`. Le `true` ne garantit que le **déclenchement**, pas
> l'aboutissement. `nullptr` → `false`. **Piège de sécurité** : pas de validation de l'argument, et
> Linux/macOS passent par `system()` → ne jamais transmettre un input utilisateur non vérifié (risque
> d'**injection de commande**).

---

## Aperçu de l'API

Tous les éléments publics des deux headers, en un coup d'œil. Le détail (comportement, cas d'usage)
suit dans la « Référence complète ».

### `NkDialogs.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Résultat | `struct NkDialogResult` | Agrégat POD de retour des dialogues. |
| Résultat | `.confirmed` (`bool`, `false`) | `true` si l'utilisateur a validé. |
| Résultat | `.path` (`NkString`, vide) | Chemin sélectionné (file dialogs), ownership interne. |
| Résultat | `.color` (`uint32`, `0`) | Couleur RGBA choisie (color picker). |
| Fichier | `static NkDialogResult OpenFileDialog(filter = "*.*", title = "Open File")` | Sélecteur d'ouverture → `confirmed` + `path`. |
| Fichier | `static NkDialogResult SaveFileDialog(defaultExt = "", title = "Save File")` | Sélecteur de sauvegarde (ext **sans point**) → `confirmed` + `path`. |
| Message | `static void OpenMessageBox(message, title = "Message", type = 0)` | Boîte de message (`type` 0/1/2), **ne renvoie rien**. |
| Couleur | `static NkDialogResult ColorPicker(initial = 0xFFFFFFFF)` | Sélecteur de couleur → `confirmed` + `color`. |

Aucune méthode de `NkDialogs` n'est `noexcept` ni `nodiscard`. Paramètres texte en `const NkString&`.

### `NkLauncher.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Ouverture | `static bool OpenURL(const char* url) noexcept` | URL dans le navigateur par défaut. |
| Ouverture | `static bool OpenFile(const char* filePath) noexcept` | Fichier dans l'application associée. |
| Ouverture | `static bool OpenFolder(const char* folderPath) noexcept` | Dossier dans l'explorateur système. |

Toutes `noexcept`, arguments `const char*`, retour `true` = invocation déclenchée (pas aboutissement).

---

## Référence complète

Chaque élément est repris en détail : comportement, retour, et usages dans les différents domaines du
moteur. Les éléments triviaux sont brefs ; les pièges réels (blocage, sécurité, `confirmed`) sont
traités à fond.

### `NkDialogResult` — le résultat partagé

C'est un agrégat POD : on le construit par valeur de retour, on lit ses trois champs, le destructeur
libère la `NkString path`. Il n'a **ni constructeur explicite, ni Create/Destroy** : la mémoire de la
chaîne est gérée par le conteneur maison lui-même. Tout l'art tient en une discipline : `confirmed`
est le **garde** ; `path` et `color` ne sont fiables que lorsqu'il vaut `true`.

- **Outils / éditeur** — c'est le domaine roi : `path` alimente l'ouverture/sauvegarde de projets,
  scènes, assets ; `color` alimente un sélecteur de teinte d'un inspecteur.
- **IO / réseau** — `path` désigne le fichier à lire/écrire avant de lancer un import, un export, un
  envoi.
- **UI / 2D** — `color` se reverse directement dans un pinceau, une teinte de calque, un thème.

### `OpenFileDialog` — sélectionner un fichier à lire

Méthode `static` bloquante, retour `NkDialogResult`. Le `filter` (`"*.png;*.jpg"`, vide = tout) borne
les extensions visibles, le `title` nomme la fenêtre. Au retour, `confirmed` indique la validation,
`path` porte le chemin **possédé** par le résultat. Le coût n'a pas de complexité algorithmique
pertinente : c'est un appel système modal, dont la durée est celle de l'interaction utilisateur.

- **Outils / éditeur** — ouvrir une scène `.nkscene`, un projet `.nkproj`, importer un modèle ou une
  texture : on filtre par extension, on charge si `confirmed`.
- **Rendu** — choisir un fichier de shader, une LUT, une HDRI à brancher dans le pipeline.
- **Audio** — sélectionner un son ou une banque à charger via NKAudio.
- **IO** — point d'entrée standard de tout import de données externes.

### `SaveFileDialog` — choisir où écrire

Symétrique de l'ouverture, en `static` bloquant, retour `NkDialogResult`. Le piège à mémoriser est
`defaultExt` : c'est l'extension **sans point** (`"png"`), proposée par défaut au nom de fichier. On
écrit dans `path` seulement si `confirmed`.

- **Outils / éditeur** — « Enregistrer sous… » d'une scène, export d'un asset, sauvegarde d'une
  configuration ou d'un layout d'interface.
- **Rendu** — exporter une capture d'écran, un rendu offline, un atlas généré.
- **IO / réseau** — choisir la cible d'un dump, d'un log exporté, d'un fichier de session.

### `OpenMessageBox` — informer l'utilisateur

L'exception du lot : elle **ne renvoie rien**, elle ne capture aucun choix. Son `type` est un `int`
brut — `0` info, `1` warning, `2` error — sans enum, donc on écrit les littéraux. C'est l'outil de la
**notification ponctuelle**, pas du dialogue de confirmation.

- **Outils / éditeur** — signaler une sauvegarde réussie, une erreur de chargement, un format non
  supporté.
- **Gameplay** — alerter d'une condition de fin, d'une ressource manquante au lancement.
- **IO / réseau** — remonter un échec de lecture, de connexion, de permission, avec le bon niveau de
  gravité (warning vs error).
- **GPU / rendu** — prévenir qu'un backend n'est pas disponible, qu'une feature GPU manque, avant de
  basculer sur un fallback.

### `ColorPicker` — choisir une couleur

`static`, bloquant, retour `NkDialogResult` (`confirmed` + `color`). `initial` est la couleur de
départ RGBA (défaut blanc opaque `0xFFFFFFFF`). Le résultat `color` est un `uint32` RGBA directement
exploitable.

- **Outils / éditeur** — éditer la couleur d'une lumière, d'un matériau, d'un calque, d'un thème dans
  un inspecteur.
- **UI / 2D** — définir la teinte d'un trait, d'un fond, d'un pinceau.
- **Rendu** — fixer une couleur de clear, une couleur d'émission, une teinte de debug.

### `OpenURL`, `OpenFile`, `OpenFolder` — déléguer à l'OS

Les trois méthodes de `NkLauncher` partagent la même mécanique : `const char*` en entrée, `noexcept`,
et retour `bool` dont le `true` ne garantit que le **déclenchement** de l'invocation système — jamais
l'aboutissement côté utilisateur. Un `nullptr` est traité comme une erreur (`false`). Le booléen est
**significatif** : on le teste pour basculer sur un message d'erreur en cas d'échec.

- **Outils / éditeur** — `OpenURL` vers la documentation, le dépôt, un rapport de bug ; `OpenFolder`
  vers le dossier du projet, des captures, des logs ; `OpenFile` pour ouvrir un export dans son
  application native (un PDF de rapport, une image générée).
- **IO** — laisser le système choisir la bonne application selon l'extension plutôt que d'embarquer un
  viewer.
- **Gameplay / UI** — bouton « Site officiel », « Crédits », « Signaler un bug » qui ouvre une page
  web.
- **Réseau** — ouvrir une URL d'authentification ou de paiement dans le navigateur par défaut.

**Le piège de sécurité, à fond.** Aucune des trois méthodes ne **valide** ni ne **nettoie** son
argument. Sur Linux et macOS, l'implémentation passe par `system()` : transmettre une chaîne issue
d'un **input utilisateur non vérifié** ouvre la porte à une **injection de commande**. L'appelant est
seul responsable de fournir une valeur de confiance — chemin/URL construit par le programme, ou
fortement validé avant l'appel. Ne jamais brancher directement un champ texte saisi par l'utilisateur
sur `OpenURL`/`OpenFile`/`OpenFolder`.

### Le socle commun

- **Deux classes statiques, aucun objet.** Ni `NkDialogs` ni `NkLauncher` ne s'instancient : pas de
  ctor/dtor, pas d'état, pas de Create/Destroy. On appelle toujours `NkClasse::Methode(...)`.
- **Aucun enum, aucune macro, aucune free function** publics dans ces deux headers ; juste les
  méthodes statiques et le struct `NkDialogResult`.
- **Deux conventions d'API distinctes.** `NkDialogs` parle `NkString` et **n'est pas** `noexcept` (ses
  dialogues bloquent et manipulent des chaînes maison) ; `NkLauncher` parle `const char*` et est
  **entièrement `noexcept`** (de simples invocations système qui ne lèvent pas).
- **Synchrone côté dialogue, asynchrone côté résultat.** Les `NkDialogs` bloquent jusqu'à la réponse
  de l'utilisateur ; les `NkLauncher` rendent la main dès le déclenchement, sans attendre l'app cible.

---

### Exemple récapitulatif

```cpp
#include "NKWindow/Core/NkDialogs.h"
#include "NKWindow/Core/NkLauncher.h"
using namespace nkentseu;

// 1. Ouvrir une scène choisie par l'utilisateur.
NkDialogResult open = NkDialogs::OpenFileDialog("*.nkscene", "Ouvrir une scène");
if (open.confirmed)
    LoadScene(open.path);                       // path n'est lu que si confirmed

// 2. Enregistrer sous, extension par défaut sans point.
NkDialogResult save = NkDialogs::SaveFileDialog("nkscene", "Enregistrer sous");
if (save.confirmed)
    SaveScene(save.path);

// 3. Choisir une teinte de lumière (RGBA, blanc au départ).
NkDialogResult col = NkDialogs::ColorPicker(0xFFFFFFFF);
if (col.confirmed)
    light.tint = col.color;

// 4. Informer en cas d'erreur (type 2 = error, sans retour).
NkDialogs::OpenMessageBox("Backend GPU indisponible.", "Erreur", 2);

// 5. Déléguer à l'OS — tester le booléen, ne jamais passer d'input non validé.
if (!NkLauncher::OpenURL("https://docs.nkentseu.dev"))
    NkDialogs::OpenMessageBox("Impossible d'ouvrir le navigateur.", "Erreur", 2);
NkLauncher::OpenFolder("Captures");             // explorateur sur le dossier des captures
```

---

[← Index NKWindow](README.md) · [Récap NKWindow](../NKWindow.md) · [Couche Runtime](../README.md)
