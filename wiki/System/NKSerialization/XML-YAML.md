# XML et YAML : lecteurs et écrivains

> Couche **System** · NKSerialization · Transformer une `NkArchive` en texte **XML** ou **YAML**
> (et l'inverse) : les paires `NkXMLReader`/`NkXMLWriter` et `NkYAMLReader`/`NkYAMLWriter`.

À un moment, tout ce qu'un moteur garde en mémoire — une scène, un réglage, un profil de joueur —
doit **descendre sur disque** ou **partir sur le réseau** sous une forme qu'un humain ou une autre
machine peut relire. NKSerialization répond à ce besoin avec un type pivot, la **`NkArchive`** : un
arbre de valeurs (objets, tableaux, scalaires) indépendant de tout format. La question n'est donc
jamais « comment ranger mes données » — l'archive s'en charge — mais « **dans quel texte** je veux
les écrire, et **comment** les relire ». Cette page couvre deux dialectes : le **XML**, balisé et
verbeux, fait pour l'interopérabilité et les outils ; le **YAML**, épuré et indenté, fait pour les
fichiers de configuration qu'on édite à la main.

Les quatre classes fonctionnent **par paires symétriques** : un *Reader* parse du texte vers une
archive, un *Writer* sérialise une archive vers du texte. Aucune ne possède de données : elles ne
sont que des **boîtes à outils statiques**. On ne les instancie pas, on ne les copie pas — on
appelle directement `NkXMLWriter::WriteArchive(...)`. C'est l'`NkArchive` et le `NkString` de sortie
qui vous appartiennent ; les quatre classes se contentent de les remplir.

- **Namespace** : `nkentseu`
- **Headers** :
  `#include "NKSerialization/XML/NkXMLReader.h"`,
  `#include "NKSerialization/XML/NkXMLWriter.h"`,
  `#include "NKSerialization/YAML/NkYAMLReader.h"`,
  `#include "NKSerialization/YAML/NkYAMLWriter.h"`
- **Type pivot** : `NkArchive` (`#include "NKSerialization/NkArchive.h"`)

---

## Écrire une archive en XML : `NkXMLWriter`

Une fois l'archive peuplée, l'écrire en XML est une seule ligne. `NkXMLWriter` parcourt l'arbre
**récursivement** et produit un document dont la racine est toujours `<archive>`, chaque scalaire
devenant une `<entry>`, chaque objet un `<object>`, chaque tableau un `<array>`. Le point important
— et ce qui distingue ce writer d'un export XML « décoratif » — est qu'il **préserve le type** : un
attribut `type=` (`null`, `bool`, `int64`, `uint64`, `float64`, `string`) accompagne chaque valeur,
pour qu'un *round-trip* écriture → lecture redonne exactement les mêmes types, pas seulement les
mêmes textes. Le contenu est par ailleurs **échappé** (`&`, `<`, `>`, `"`, `'` → entités), donc le
document reste valide même si une chaîne contient des caractères réservés.

```cpp
NkArchive doc;
doc.SetString("name", "Hero");
doc.SetInt32("level", 7);

NkString xml;
NkXMLWriter::WriteArchive(doc, xml);          // pretty par défaut, indentation 2 espaces
```

Deux réglages pilotent la mise en forme. `pretty` (défaut `true`) ajoute sauts de ligne et
indentation pour un fichier **lisible** ; à `false`, tout sort sur **une seule ligne** compacte,
idéal pour un payload réseau où chaque octet compte. `indentSpaces` (défaut `2`) fixe le nombre
d'espaces par niveau quand `pretty` est actif.

Ce n'est **pas** un sérialiseur XML générique : il n'écrit que ce que `NkXMLReader` sait relire.
Et il existe en deux formes qu'il faut distinguer. La surcharge à **buffer fourni**
(`WriteArchive(archive, outXml, ...)`) écrit dans un `NkString&` que vous possédez — elle le `Clear()`
au début, donc inutile de le vider vous-même — et c'est celle à privilégier dans une boucle, car
elle **réutilise** votre buffer. La surcharge **one-shot** (`WriteArchive(archive, ...)`) renvoie un
`NkString` tout neuf : pratique pour un appel isolé, mais elle **alloue** à chaque fois, donc à
éviter dans le code chaud.

> **En résumé.** `NkXMLWriter::WriteArchive` sérialise une `NkArchive` en XML typé (`type=` pour le
> round-trip), racine `<archive>`, en `O(n)` sur le nombre de nœuds. `pretty`/`indentSpaces`
> contrôlent la forme. Préférez la surcharge à buffer `NkString&` (réutilisée, vidée seule) à la
> variante one-shot (qui alloue) dans les boucles.

---

## Relire un XML : `NkXMLReader`

Le chemin inverse est tout aussi direct. `NkXMLReader::ReadArchive` prend une `NkStringView` sur le
texte XML et remplit une `NkArchive` (qu'il **`Clear()`** d'abord, comme partout). Il cherche le tag
racine `<archive>` — son absence est une **erreur** — puis descend récursivement dans les `<entry>`,
`<object>` et `<array>`, en *déséchappant* les entités (`&amp;` → `&`, `&lt;` → `<`, etc.) et en
restituant le type d'origine grâce à l'attribut `type=`.

```cpp
NkArchive doc;
NkString  err;
if (!NkXMLReader::ReadArchive(xmlText, doc, &err)) {
    NkLog::Error("XML invalide : {}", err);
    return;
}
NkString name = doc.GetString("name");        // accès via l'API de NkArchive
```

Le parser est **tolérant, non validant** : un élément inconnu ou mal formé est simplement **ignoré**
plutôt que de tout faire échouer. C'est volontaire — on veut charger ce qui est lisible —, mais cela
a une conséquence à garder en tête : en cas d'échec, l'archive peut rester **partiellement remplie**.
Ne la considérez pas valide tant que `ReadArchive` n'a pas renvoyé `true`. Le paramètre `outError`
est optionnel (`nullptr` par défaut) ; quand il est fourni et qu'une erreur survient, il reçoit un
message formaté.

Ce lecteur a des **limites assumées** : pas de `xmlns` (espaces de noms), pas de CDATA ni
d'instructions de traitement, et tout attribut autre que `key`/`type`/`count` est ignoré. Il n'est
pas là pour avaler n'importe quel XML du monde — seulement celui que `NkXMLWriter` produit.

> **En résumé.** `NkXMLReader::ReadArchive(xml, outArchive, outError)` parse en `O(n × d)` (taille ×
> profondeur), restitue les types via `type=`, déséchappe les entités. Tolérant : ignore l'inconnu,
> peut laisser une archive **partielle** sur erreur — toujours tester le `bool` de retour. Ne lit que
> le XML maison (pas de xmlns/CDATA/PI).

---

## Écrire une archive en YAML : `NkYAMLWriter`

Quand le fichier est destiné à être **édité à la main** — un réglage, un descripteur de niveau, un
profil — le YAML est plus agréable que le XML : pas de balises, juste de l'indentation et des
`clé: valeur`. `NkYAMLWriter::WriteArchive` produit ce texte lisible, récursivement, avec un marqueur
`---` en tête de document. Les `null` deviennent `~`, les booléens et les nombres sont écrits **sans
guillemets**, et les clés contenant des espaces ou des caractères spéciaux sont **quotées
automatiquement** ; à l'intérieur d'une chaîne single-quote, un `'` est échappé en `''`.

```cpp
NkArchive cfg;
cfg.SetBool("fullscreen", true);
cfg.SetInt32("width", 1920);

NkString yaml;
NkYAMLWriter::WriteArchive(cfg, yaml);        // toujours pretty, 2 espaces, '---' en tête
```

Une **asymétrie** avec le writer XML mérite d'être notée : le YAML writer n'a **aucune option**. Pas
de `pretty`, pas d'`indentSpaces` — la mise en forme est figée à **2 espaces par niveau**, toujours
lisible, toujours préfixée de `---`. C'est un choix : le YAML n'existe que pour être lu par un humain,
une variante compacte n'aurait pas de sens. Comme le writer XML, il existe en deux formes : la
surcharge à **buffer** (`WriteArchive(archive, outYaml)`, vidé seul, réutilisable) à préférer en
boucle, et la **one-shot** (`WriteArchive(archive)`) qui alloue un nouveau `NkString` à chaque appel.

> **En résumé.** `NkYAMLWriter::WriteArchive` écrit une `NkArchive` en YAML lisible (`---` en tête,
> `~` pour null, indentation **fixe 2 espaces**, pas d'options), en `O(n)`. Surcharge à buffer
> `NkString&` réutilisée vs one-shot qui alloue. Contrairement au XML, aucun réglage de forme.

---

## Relire un YAML : `NkYAMLReader`

`NkYAMLReader::ReadArchive` lit le YAML **ligne par ligne**, en se laissant guider par
l'**indentation** pour reconstruire la hiérarchie dans l'archive (qu'il `Clear()` d'abord). Il
comprend les *block mappings* (`clé: valeur`), les *block sequences* (`- item`) et les scalaires :
`null`/`~` (insensible à la casse) deviennent une valeur nulle, `true`/`false` un booléen, et les
nombres sont reconnus via les conversions entières/flottantes — avec **repli sur chaîne** si la
conversion échoue. Les chaînes peuvent être quotées en simple ou double, `''` redonnant un `'`.

```cpp
NkArchive cfg;
NkString  err;
if (!NkYAMLReader::ReadArchive(yamlText, cfg, &err)) {
    NkLog::Error("YAML invalide : {}", err);
    return;
}
bool fs = cfg.GetBool("fullscreen");
```

Le lecteur **saute** ce qui ne porte pas de données : les marqueurs de document `---` et `...`, les
commentaires `#`, les lignes vides. Comme le reader XML, il est non validant et peut laisser une
archive **partielle** en cas d'échec — on teste toujours le `bool`, et on lit `*outError` (optionnel)
pour le diagnostic. Ses **limites** sont à connaître pour ne pas écrire un YAML qu'il ne saura pas
relire : pas de *flow styles* `{ }` / `[ ]`, pas de chaînes multi-lignes `|` / `>`, et les tableaux
imbriqués sont **simplifiés** (ignorés ou réduits à `[]`). Tant qu'on reste dans le sous-ensemble que
`NkYAMLWriter` produit, le round-trip est garanti.

> **En résumé.** `NkYAMLReader::ReadArchive(yaml, outArchive, outError)` parse par indentation en
> `O(n × d)` (lignes × profondeur) : mappings, séquences, scalaires (`~`/null, booléens, nombres avec
> repli string). Saute `---`/`...`/`#`/vides. Non validant, archive **partielle** possible sur erreur.
> Pas de flow styles, ni multi-lignes, ni tableaux imbriqués riches.

---

## Aperçu de l'API

Les quatre classes sont **purement statiques** (aucun constructeur, aucun état, doc-comment
thread-safe). Toutes les méthodes sont `static` et signalent l'échec par un `nk_bool` de retour (pas
d'exceptions). Complexités entre crochets.

### `NkXMLWriter` — archive → texte XML

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Écriture (buffer) | `static nk_bool WriteArchive(const NkArchive&, NkString& outXml, nk_bool pretty = true, nk_int32 indentSpaces = 2)` `[O(n)]` | Sérialise dans le `NkString` fourni (vidé via `Clear()`). `pretty` = lisible / compact ; `indentSpaces` = espaces par niveau. À préférer en boucle. |
| Écriture (one-shot) | `static NkString WriteArchive(const NkArchive&, nk_bool pretty = true, nk_int32 indentSpaces = 2)` `[O(n)]` | Variante retournant un nouveau `NkString` (alloue — à éviter en boucle serrée). |

### `NkXMLReader` — texte XML → archive

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Lecture | `static nk_bool ReadArchive(NkStringView xml, NkArchive& outArchive, NkString* outError = nullptr)` `[O(n × d)]` | Parse le XML dans l'archive (vidée via `Clear()`). Racine `<archive>` requise. Tolérant : ignore l'inconnu. `outError` optionnel. |

### `NkYAMLWriter` — archive → texte YAML

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Écriture (buffer) | `static nk_bool WriteArchive(const NkArchive&, NkString& outYaml)` `[O(n)]` | Sérialise dans le `NkString` fourni (vidé via `Clear()`). Marqueur `---` en tête, indentation **fixe 2 espaces**. À préférer en boucle. |
| Écriture (one-shot) | `static NkString WriteArchive(const NkArchive&)` `[O(n)]` | Variante retournant un nouveau `NkString` (`---` inclus ; alloue — à éviter en boucle). |

### `NkYAMLReader` — texte YAML → archive

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Lecture | `static nk_bool ReadArchive(NkStringView yaml, NkArchive& outArchive, NkString* outError = nullptr)` `[O(n × d)]` | Parse le YAML dans l'archive (vidée via `Clear()`). Piloté par l'indentation. Saute `---`/`...`/`#`/vides. `outError` optionnel. |

### Transverse

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Export | `NKENTSEU_SERIALIZATION_CLASS_EXPORT` | Macro d'export (déclarée dans `NkSerializationApi.h`), appliquée à chaque classe. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, et usages dans les différents
domaines du moteur. Les écritures one-shot, triviales, sont décrites brièvement ; les lecteurs et la
mécanique de round-trip le sont **à fond**.

### Le modèle commun : archive ↔ texte

Avant les méthodes, le principe. Ces quatre classes ne **stockent** rien : ce sont des fonctions
statiques groupées en classe. Le travail réel se passe sur deux objets qui vous appartiennent —
l'`NkArchive` (l'arbre de données) et le `NkString` de sortie. Aucune n'alloue de structure
persistante, aucune n'expose de `new`/`delete` : pas de RAII à gérer de votre côté pour elles.

Le contrat est **symétrique**. Côté écriture, on **peuple** une archive avec son API (`SetString`,
`SetInt32`, `SetBool`, `SetObject`, `SetArray`, `SetNull`…), puis on appelle un `WriteArchive`. Côté
lecture, on appelle `ReadArchive`, puis on **interroge** l'archive (`GetString`, `GetFloat64`,
`GetObject`, `Has`…). Un détail qui simplifie la vie : **tous** les buffers et archives de sortie
(`outXml`, `outYaml`, `outArchive`) sont **`Clear()` en début d'opération** — pas besoin de les vider
manuellement.

- **Outils / éditeur** — un inspecteur sauvegarde l'état d'un objet : peuplez l'archive depuis les
  champs édités, écrivez en XML pour un format d'échange ou en YAML pour un fichier que l'utilisateur
  pourra rouvrir dans un éditeur de texte.
- **IO / réseau** — un message se sérialise en XML compact (`pretty=false`) côté émetteur, se relit en
  archive côté récepteur via `ReadArchive` ; le type pivot `NkArchive` rend les deux bouts indépendants
  du protocole.
- **Gameplay** — un profil de joueur ou une sauvegarde de partie est une archive qu'on dépose en YAML
  lisible puis qu'on recharge au lancement.

### `NkXMLWriter::WriteArchive` (deux surcharges) — sérialiser en XML

La surcharge à **buffer** écrit dans le `NkString&` fourni (qu'elle `Clear()` d'abord) et renvoie
`true`/`false`. Elle produit un document racine `<archive>`, mappe chaque scalaire sur une `<entry>`
typée, chaque objet sur `<object>`, chaque tableau sur `<array count=...>`, échappe les caractères
réservés, et applique l'indentation choisie. La complexité est **O(n)** sur le nombre total de nœuds
de l'archive — un seul parcours. Le `type=` posé sur chaque valeur (`null/bool/int64/uint64/float64/
string`) est ce qui rend le **round-trip exact** avec `NkXMLReader`.

La surcharge **one-shot** fait la même chose mais **retourne** un `NkString` neuf au lieu d'écrire
dans un buffer fourni — elle **alloue** ce buffer, donc à réserver aux appels isolés.

- **Outils / éditeur** — exporter une scène ou un asset dans un format balisé que d'autres outils
  (diff, validation, transformation) savent manipuler ; `pretty=true` pour un fichier relisible et
  *diff-able* sous contrôle de version.
- **IO / réseau** — `pretty=false` produit un payload sur une ligne, plus compact à transmettre ;
  réutilisez la surcharge à buffer pour ne pas allouer à chaque message envoyé.
- **Threading** — la classe étant sans état, plusieurs threads peuvent sérialiser **en parallèle**,
  à condition que **chacun** écrive dans **son propre** `NkString` (ne partagez jamais le même buffer
  entre threads).

### `NkXMLReader::ReadArchive` — désérialiser du XML

Méthode unique. Elle prend une `NkStringView` (vue non-possédante sur le texte) et remplit
`outArchive` (vidée d'abord), en cherchant la racine `<archive>` — **absente, c'est une erreur** —
puis en descendant récursivement, en restituant les types via `type=` et en déséchappant les entités
XML. Retour `true`/`false` ; `outError` (optionnel) reçoit un message formaté en cas d'échec. La
complexité est **O(n × d)** : taille du XML × profondeur maximale d'imbrication.

Deux comportements à intégrer dans votre code. D'abord, le parser est **tolérant** : éléments
inconnus ou mal formés **ignorés**, jamais d'arrêt brutal — vous chargez ce qui est lisible. Ensuite,
corollaire direct : sur échec, l'archive peut être **partiellement** peuplée, donc **ne l'utilisez
pas** tant que le retour n'est pas `true`.

- **Outils / éditeur** — réimporter un fichier exporté par `NkXMLWriter`, ou ingérer un XML produit
  par une chaîne externe **compatible** (racine `<archive>`, attributs `key`/`type`/`count`).
- **IO / réseau** — décoder un message reçu ; la tolérance évite qu'un champ futur inconnu (ajouté par
  une version plus récente de l'émetteur) ne casse le décodage côté ancien récepteur.
- **Gameplay** — recharger une sauvegarde ; testez systématiquement le `bool` avant de lire les champs,
  une archive partielle menant sinon à des valeurs par défaut silencieuses.

### `NkYAMLWriter::WriteArchive` (deux surcharges) — sérialiser en YAML

La surcharge à **buffer** écrit dans le `NkString&` fourni (vidé d'abord), pose un marqueur `---` en
tête, et indente à **2 espaces par niveau** — sans option, contrairement au writer XML. `null` →
`~`, booléens et nombres non quotés, clés à espaces/spéciaux quotées automatiquement, `'` échappé en
`''` dans les single-quotes. Complexité **O(n)** sur le nombre de nœuds. La surcharge **one-shot**
retourne un nouveau `NkString` (`---` inclus) et **alloue** — à éviter en boucle.

- **Outils / éditeur** — produire un fichier de configuration que l'utilisateur ouvre et modifie dans
  un éditeur de texte ; l'absence de balises et l'indentation 2 espaces donnent un rendu propre et
  stable (donc *diff-able*).
- **Gameplay** — déposer des réglages, descripteurs de niveau ou paramètres d'équilibrage que les
  designers ajustent à la main entre deux runs.
- **Threading** — comme pour le XML : sérialisation concurrente sûre tant que chaque thread a son
  propre `NkString`.

### `NkYAMLReader::ReadArchive` — désérialiser du YAML

Méthode unique, **pilotée par l'indentation**. Elle lit ligne par ligne, reconstruit la hiérarchie
dans `outArchive` (vidée d'abord), reconnaît mappings (`clé: valeur`), séquences (`- item`) et
scalaires : `~`/`null` (insensible à la casse) → valeur nulle, `true`/`false` → booléen, nombres via
conversions entières/flottantes **avec repli sur chaîne** si l'analyse échoue ; `''` → `'` dans les
single-quotes. Elle **saute** `---`, `...`, les commentaires `#` et les lignes vides. Retour
`true`/`false`, `outError` optionnel. Complexité **O(n × d)** : nombre de lignes × profondeur
d'indentation.

Mêmes précautions que le reader XML : **non validant**, donc archive potentiellement **partielle** sur
échec — tester le `bool`. Et mêmes **limites** que son writer jumeau : pas de flow styles `{ }`/`[ ]`,
pas de multi-lignes `|`/`>`, tableaux imbriqués simplifiés.

- **Outils / éditeur** — relire le fichier de config que l'utilisateur vient de modifier ; le repli
  string sur conversion ratée évite qu'une valeur tapée à la main (« auto » au lieu d'un nombre) ne
  casse tout le chargement.
- **Gameplay** — charger réglages et descripteurs au démarrage, en restant dans le sous-ensemble que
  `NkYAMLWriter` produit pour garantir le round-trip.
- **IO** — ingérer un YAML simple venu d'ailleurs, à condition d'éviter flow styles, multi-lignes et
  tableaux imbriqués riches, non gérés ici.

### `NKENTSEU_SERIALIZATION_CLASS_EXPORT` — la macro d'export

Déclarée dans `NkSerializationApi.h` (et non dans ces quatre headers), elle est appliquée à chaque
classe pour gérer l'export/import de symboles selon la plateforme. Vous ne l'utilisez pas
directement : elle est purement interne au module et n'a d'effet que sur l'édition de liens.

### Pièges et idiomes

- **Vidage automatique.** Tous les buffers et archives de sortie sont `Clear()` en début d'opération
  — le `buffer.Clear()` manuel est **optionnel**.
- **Réutilisation des buffers.** En boucle, préférez la surcharge à `NkString&` (réutilisée) à la
  variante retournant un `NkString` (qui **alloue** à chaque appel).
- **Gestion d'erreur sans exception.** Toujours tester le `nk_bool` de retour, et lire `*outError`
  (lecteurs uniquement — les writers n'ont **pas** de paramètre d'erreur). Sur échec de lecture,
  l'archive peut être **à moitié remplie** : ne la considérez pas valide.
- **Threading.** Classes purement statiques sans état partagé → sûres en concurrence, **si** chaque
  appel travaille sur ses propres `NkArchive`/`NkString` (ne partagez pas un buffer entre threads).
- **Asymétrie de configuration.** Le writer XML expose `pretty`/`indentSpaces` ; le writer YAML
  n'offre **aucune** option (toujours pretty, 2 espaces, `---` en tête).
- **Sous-ensemble assumé.** Ces lecteurs ne sont pas universels : ils lisent ce que leurs writers
  produisent (XML : racine `<archive>`, pas de xmlns/CDATA/PI ; YAML : pas de flow styles, ni
  multi-lignes, ni tableaux imbriqués riches).

---

### Exemple récapitulatif

```cpp
#include "NKSerialization/XML/NkXMLReader.h"
#include "NKSerialization/XML/NkXMLWriter.h"
#include "NKSerialization/YAML/NkYAMLReader.h"
#include "NKSerialization/YAML/NkYAMLWriter.h"
using namespace nkentseu;

// 1. Peupler l'archive (API NkArchive).
NkArchive doc;
doc.SetString("name", "Hero");
doc.SetInt32("level", 7);
doc.SetBool("alive", true);

// 2. Écrire en XML (buffer réutilisable, pretty + indentation 2).
NkString xml;
NkXMLWriter::WriteArchive(doc, xml, /*pretty*/ true, /*indentSpaces*/ 2);

// 3. Round-trip : relire le XML dans une nouvelle archive.
NkArchive back;
NkString   err;
if (!NkXMLReader::ReadArchive(xml, back, &err)) {
    // err contient le message ; 'back' peut être partielle → ne pas l'utiliser.
}

// 4. Réexporter la même archive en YAML lisible (aucune option, '---' en tête).
NkString yaml;
NkYAMLWriter::WriteArchive(back, yaml);

// 5. Et relire ce YAML, piloté par l'indentation.
NkArchive cfg;
if (NkYAMLReader::ReadArchive(yaml, cfg, &err)) {
    NkString name = cfg.GetString("name");   // "Hero"
}
```

---

[← Index NKSerialization](README.md) · [Récap NKSerialization](../NKSerialization.md) · [Couche System](../README.md)
