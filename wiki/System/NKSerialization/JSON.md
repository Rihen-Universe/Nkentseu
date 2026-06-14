# Le format JSON

> Couche **System** · NKSerialization · Transformer un `NkArchive` en texte **JSON** lisible et
> le relire : l'écrivain `NkJSONWriter`, le lecteur `NkJSONReader`, et les deux fonctions
> d'échappement de chaînes qui font le sale boulot dessous.

Dès qu'on veut **sauver l'état du moteur sur disque** — un fichier de scène `.nkscene`, un projet
`.nkproj`, une configuration d'éditeur — il faut un format texte que l'on puisse écrire, relire,
versionner dans git et même corriger à la main dans un éditeur. JSON est ce format : structuré,
universel, lisible. La question n'est jamais « comment ranger les données » (c'est le travail de
`NkArchive`, l'arbre clé/valeur en mémoire), mais « comment **transcrire fidèlement** cet arbre en
texte, puis le reconstruire à l'identique ». Tout le compromis tient en une idée : **l'écrivain et
le lecteur sont un couple symétrique**, conçus l'un pour l'autre, qui passent tous deux par le même
jeu d'**échappement de chaînes**. Cette page vous apprend à les utiliser ensemble.

Ces trois fichiers ne définissent **aucun type valeur**. Il n'y a pas de classe `NkJSONValue`, pas
de DOM, pas d'arbre JSON à manipuler nœud par nœud (le nom de fichier `NkJSONValue.h` est
trompeur : il ne contient que deux fonctions libres d'échappement). La **donnée vit toujours dans
un `NkArchive`** que vous possédez ; le lecteur et l'écrivain ne sont que des **convertisseurs sans
état**, exposés comme classes utilitaires à méthodes statiques.

- **Namespace** : `nkentseu` (pas de sous-namespace)
- **Headers** (à inclure individuellement, aucun parapluie) :
  `#include "NKSerialization/JSON/NkJSONValue.h"`,
  `#include "NKSerialization/JSON/NkJSONReader.h"`,
  `#include "NKSerialization/JSON/NkJSONWriter.h"`

---

## Écrire un `NkArchive` en JSON : `NkJSONWriter`

C'est le point de départ : vous avez rempli un `NkArchive` en mémoire (l'arbre des données du
moteur), et vous voulez le **poser sur disque** sous forme de texte. `NkJSONWriter::WriteArchive`
fait exactement cela — il parcourt l'archive et produit le JSON correspondant. La classe est un
**utilitaire statique pur** : aucun objet à construire, aucun état interne, on appelle directement
la méthode de classe.

Deux surcharges, pour deux idiomes. La première écrit **dans un buffer que vous fournissez** (un
`NkString&` vidé au début de l'appel) et renvoie un `nk_bool` de succès ; la seconde, plus
expressive, **crée et renvoie** le `NkString` directement. Toutes deux acceptent deux options : le
drapeau `pretty` (par défaut `true`) qui choisit entre une sortie **indentée et lisible** ou un
JSON **compact sur une ligne**, et `indentSpaces` (par défaut `2`) qui règle le nombre d'espaces
par niveau d'imbrication.

```cpp
NkArchive scene = BuildSceneArchive();         // votre arbre de données

NkString json;
NkJSONWriter::WriteArchive(scene, json);        // joli, 2 espaces, dans 'json'

// Variante one-shot, compacte (pour le réseau ou un cache) :
NkString packed = NkJSONWriter::WriteArchive(scene, /*pretty*/false);
```

La seconde surcharge n'est qu'une **commodité** : elle alloue un `NkString` interne et délègue à la
première. Pratique, mais elle **alloue à chaque appel** — à fuir dans une boucle serrée. Le bon
réflexe de performance, quand on sérialise en masse (un fichier par entité, une frame par tick),
est de garder **un seul** `NkString` et de le réutiliser : la surcharge à buffer le vide
(`Clear()`) au début, donc on évite les allocations répétées.

> **En résumé.** `NkJSONWriter::WriteArchive` transcrit un `NkArchive` en texte JSON. Surcharge à
> **buffer** (`NkString&`, renvoie `nk_bool`) pour réutiliser la mémoire en boucle ; surcharge
> **par retour** (`NkString`) pour le one-shot. `pretty` = lisible vs compact, `indentSpaces` =
> indentation. Clés et chaînes sont échappées automatiquement.

---

## Relire du JSON dans un `NkArchive` : `NkJSONReader`

L'opération inverse : vous avez un texte JSON (lu d'un fichier, reçu sur le réseau) et vous voulez
le **reconstruire en `NkArchive`** pour que le moteur s'en serve. `NkJSONReader::ReadArchive` parse
le texte et **remplit une archive que vous fournissez**. Là encore, classe utilitaire statique,
aucun objet à instancier.

L'archive de destination est **vidée (`Clear()`) en début d'opération** : vous lui passez une
archive (vide ou non), elle en ressort avec le contenu du JSON, quoi qu'elle ait contenu avant. La
méthode renvoie un `nk_bool` — `true` si le parsing a réussi — et accepte un troisième paramètre
optionnel, un `NkString*` où, en cas d'échec, elle écrit un **message d'erreur détaillé** (avec la
position du problème dans le texte). Passez `nullptr` (la valeur par défaut) si l'erreur ne vous
intéresse pas.

```cpp
NkString text = ReadFileToString("scene.nkscene");

NkArchive scene;
NkString  error;
if (!NkJSONReader::ReadArchive(text, scene, &error)) {
    NK_LOG_ERROR("JSON invalide : {}", error);   // message avec position
    return;
}
// 'scene' est maintenant peuplée et prête à l'emploi
```

Ce n'est **pas** un parser JSON générique à tout faire. Il est conçu pour **relire spécifiquement
le format produit par `NkJSONWriter`** : il exige un objet racine `{`, accepte `null`,
`true`/`false` (minuscules strictes), les nombres entiers et flottants décimaux, les chaînes
échappées, et imbrique objets `{}` et tableaux `[]`. La notation scientifique avancée n'est pas
garantie, la validation des nombres est tolérante, et — point important — les séquences `\uXXXX`
ne sont **pas** réellement décodées (voir la section suivante). Dernier piège : en cas d'erreur en
cours de route, l'archive de sortie peut contenir des **données partielles** — testez toujours le
booléen de retour avant de l'utiliser.

> **En résumé.** `NkJSONReader::ReadArchive(json, outArchive, outError=nullptr)` parse du texte
> JSON dans une archive (vidée au début), renvoie `nk_bool` et remplit optionnellement un message
> d'erreur positionné. Parser récursif single-pass, **objet racine obligatoire**, pensé pour relire
> le format du Writer — pas un parser universel. Sur erreur, l'archive peut être partielle.

---

## L'échappement de chaînes : `NkJSONEscapeString` / `NkJSONUnescapeString`

Sous le Writer et le Reader vit la mécanique de bas niveau : transformer une chaîne brute en chaîne
**conforme JSON** et inversement. JSON impose que certains caractères apparaissent sous forme
échappée à l'intérieur des guillemets — un guillemet `"` doit s'écrire `\"`, une tabulation doit
devenir `\t`, etc. Ces deux fonctions libres font ce travail, et le Writer les appelle pour chaque
clé et chaque valeur texte.

`NkJSONEscapeString(input)` prend une `NkStringView` et renvoie une `NkString` **échappée**, prête
à être glissée entre guillemets. Elle traduit `"` → `\"`, `\` → `\\`, `/` → `\/`, le backspace en
`\b`, le form feed en `\f`, le saut de ligne en `\n`, le retour chariot en `\r`, la tabulation en
`\t`, et **tout** caractère de contrôle (`< 0x20`) en `\uXXXX` hexadécimal. C'est `noexcept` et en
O(n).

`NkJSONUnescapeString(input, ok)` fait l'inverse : elle décode une chaîne échappée. Comme une
chaîne échappée peut être **mal formée**, elle ne peut pas se contenter de renvoyer un résultat —
elle prend un `nk_bool&` en sortie. L'idiome est immuable : déclarez un booléen, passez-le par
référence, et **testez-le toujours** avant d'utiliser le résultat.

```cpp
NkString esc = NkJSONEscapeString(R"(chemin\fichier "x")");   // chemin\\fichier \"x\"

nk_bool ok = false;
NkString raw = NkJSONUnescapeString(esc, ok);
if (!ok) { /* échappement invalide, 'raw' est vide */ }
```

Attention à une **asymétrie volontaire** : l'échappement produit du `\uXXXX` pour les caractères de
contrôle, mais le décodage des `\uXXXX` n'est que **minimal** — il émet un `'?'` au lieu du vrai
code point Unicode (tout en mettant `ok=true`). C'est une **perte d'information silencieuse** :
faire un aller-retour échappe/dé-échappe sur du texte contenant des contrôles non standard ne
restitue pas l'original. Pour le texte ASCII et les guillemets/anti-slashs/sauts de ligne usuels,
le couple est parfaitement réversible.

> **En résumé.** `NkJSONEscapeString` (chaîne → forme JSON, `noexcept`, O(n)) et
> `NkJSONUnescapeString` (forme JSON → chaîne, via `nk_bool& ok`, O(n)) sont la mécanique sous le
> Writer/Reader. Réversibles pour l'ASCII courant ; **piège** : `\uXXXX` rend `'?'` au décodage —
> pas de vrai Unicode, perte silencieuse.

---

## Aperçu de l'API

Les seuls symboles publics réels : **2 fonctions libres** d'échappement et **2 classes utilitaires
statiques** (Reader / Writer). Aucun type DOM, aucun enum, aucune macro fonction, aucun opérateur.
Complexités / qualificateurs entre crochets.

### `NkJSONValue.h` — fonctions d'échappement (libres)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Échappement | `NkJSONEscapeString(NkStringView)` `[noexcept, O(n)]` | Chaîne brute → chaîne conforme JSON (renvoie `NkString`). |
| Dé-échappement | `NkJSONUnescapeString(NkStringView, nk_bool& ok)` `[noexcept, O(n)]` | Chaîne JSON → texte brut ; `ok` = succès (renvoie `NkString`). |

### `NkJSONWriter.h` — écriture (classe statique)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Écriture (buffer) | `static NkJSONWriter::WriteArchive(const NkArchive&, NkString& out, nk_bool pretty=true, nk_int32 indentSpaces=2)` `[O(n)]` | Sérialise dans un buffer fourni (vidé au début) ; renvoie `nk_bool`. |
| Écriture (retour) | `static NkJSONWriter::WriteArchive(const NkArchive&, nk_bool pretty=true, nk_int32 indentSpaces=2)` `[O(n)]` | Variante one-shot ; renvoie un `NkString` JSON. |

### `NkJSONReader.h` — lecture (classe statique)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Lecture | `static NkJSONReader::ReadArchive(NkStringView json, NkArchive& out, NkString* outError=nullptr)` `[O(n)]` | Parse le JSON dans l'archive (vidée au début) ; renvoie `nk_bool`, remplit l'erreur si fournie. |

---

## Référence complète

Chaque symbole est repris à fond : comportement, complexité, et cas d'usage dans les différents
domaines du moteur. Les éléments triviaux sont brefs ; les pièges et le couple Reader/Writer le
sont en détail.

### `NkJSONWriter::WriteArchive` — sérialiser

Les deux surcharges produisent le même texte ; elles ne diffèrent que par la **gestion du buffer**.
La première écrit dans un `NkString&` qu'elle vide (`Clear()`) au début et renvoie un `nk_bool` de
succès. La seconde alloue un `NkString` interne, **délègue** à la première, et le renvoie par
valeur. Toutes deux sont en O(n) (n = nombre de caractères en sortie) et « noexcept-friendly » : pas
d'exception levée, les erreurs passent par le booléen. La classe est **thread-safe** car sans état
mutable — le buffer de sortie appartient à l'appelant.

Le rendu se règle par `pretty` (sauts de ligne + indentation `indentSpaces` si `true`, JSON compact
sur une ligne si `false`) ; les clés d'objet et les valeurs texte sont échappées via
`NkJSONEscapeString`. Usages, par domaine :

- **Outils / éditeur** — sauver un projet `.nkproj`, une scène `.nkscene`, un layout d'éditeur :
  `pretty=true` pour que le fichier reste lisible et **diffable dans git** à la main.
- **IO / réseau** — sérialiser un état pour l'envoyer ou le mettre en cache : `pretty=false` pour
  compacter, en réutilisant le buffer (surcharge 1) afin d'éviter les allocations par message.
- **ECS / scène** — exporter l'arbre de composants d'une entité (préalablement assemblé dans un
  `NkArchive`) vers un format inspectable.
- **Gameplay** — écrire des fichiers de sauvegarde, des tables de config (équilibrage, niveaux)
  que les designers relisent et corrigent sans recompiler.
- **Audio / animation** — exporter des banques de sons, des courbes ou des clips décrits en
  paramètres clé/valeur, pour un pipeline d'assets versionné.
- **Threading** — comme la classe est sans état, plusieurs threads peuvent sérialiser
  **simultanément**, chacun vers son propre `NkString`.

Le **piège perf** de la surcharge par retour : elle alloue un `NkString` à chaque appel. Dans une
boucle serrée (un fichier par entité, une sérialisation par frame), préférez la surcharge à buffer
et réutilisez le même `NkString` — le `Clear()` interne le remet à zéro sans relibérer la capacité.

### `NkJSONReader::ReadArchive` — désérialiser

Méthode statique unique. Elle prend le texte (`NkStringView`), une archive de destination
(`NkArchive&`, **vidée au début**) et un pointeur d'erreur optionnel (`NkString*`, défaut
`nullptr`). Retour `nk_bool`. Elle n'est **pas** `noexcept` au sens C++ mais « noexcept-friendly » :
pas d'exception, les erreurs remontent par le booléen et le message. C'est un **parser récursif
single-pass** en O(n), **thread-safe** (aucun état mutable ; la destination appartient à
l'appelant).

Ce qu'il accepte : un **objet racine `{` obligatoire**, des objets `{}` et tableaux `[]` imbriqués,
les scalaires `null`, `true`/`false` (minuscules strictes), les nombres (entiers signés/non signés
et flottants décimaux), les chaînes échappées. Le whitespace (espace, tab, newline, CR) entre
tokens est ignoré. Usages, par domaine :

- **Outils / éditeur** — recharger un projet ou une scène ; le `outError` positionné permet
  d'**afficher la ligne fautive** quand un fichier édité à la main est cassé.
- **IO / réseau** — décoder un message ou un état reçu, puis le rejouer dans le moteur via
  l'archive reconstruite.
- **Gameplay** — relire une sauvegarde, charger une config de niveau au démarrage.
- **ECS / scène** — reconstruire l'arbre de composants d'une entité depuis le disque.
- **Pipeline d'assets** — réimporter une banque audio, un set d'animations, des paramètres de
  matériau décrits en JSON.

Trois pièges à garder en tête. D'abord, ce **n'est pas un parser JSON générique** : il vise à
relire ce que `NkJSONWriter` a écrit — la notation scientifique avancée n'est pas garantie et la
validation des nombres est tolérante. Ensuite, `\uXXXX` rend `'?'` (cf. l'échappement). Enfin, sur
erreur en cours de parsing, l'**archive de sortie peut être partielle** — ne l'utilisez qu'après
avoir vérifié le `nk_bool` de retour.

### `NkJSONEscapeString` — échapper

Fonction libre exportée, `noexcept`, O(n). Elle prend une `NkStringView`, pré-alloue
`Reserve(longueur + 8)`, et renvoie une `NkString` où chaque caractère problématique est remplacé
par sa séquence JSON : `"` → `\"`, `\` → `\\`, `/` → `\/`, backspace → `\b`, form feed → `\f`,
newline → `\n`, CR → `\r`, tab → `\t`, et tout contrôle `< 0x20` → `\uXXXX`. C'est la brique
qu'appelle `NkJSONWriter` pour chaque clé et chaque chaîne. Usages :

- **IO / outils** — préparer un chemin de fichier Windows (`C:\dossier`) ou un texte multi-lignes
  pour qu'il survive à l'insertion dans un fichier JSON.
- **UI / 2D** — sérialiser des libellés, des messages, des descriptions saisis par l'utilisateur,
  qui contiennent volontiers des guillemets et des retours à la ligne.
- **Réseau** — encoder une charge texte avant de la placer dans un message JSON.

### `NkJSONUnescapeString` — dé-échapper

Symétrique de la précédente, `noexcept`, O(n). Elle pré-alloue `Reserve(longueur)`, décode `\"`,
`\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, et renvoie la chaîne brute. Comme l'entrée peut être
invalide, elle expose son succès via un `nk_bool& ok` : déclarez-le, passez-le par référence, et
**testez-le** avant d'utiliser le résultat. Elle pose `ok=false` et renvoie une chaîne **vide** si
elle rencontre un `\` en fin de chaîne (échappement incomplet), un `\uXXXX` avec moins de 4
chiffres hexadécimaux, ou une séquence d'échappement inconnue. Usages :

- **IO / éditeur** — reconstituer un chemin ou un texte multi-lignes relu d'un fichier JSON.
- **UI / réseau** — restaurer un libellé ou un message reçu sous forme échappée.

Le **piège** central : `\uXXXX` n'est que partiellement géré — il émet `'?'` (et laisse `ok=true`),
sans convertir vers le vrai Unicode/UTF-8. Sur du texte de contrôle non standard, l'aller-retour
échappe/dé-échappe **perd silencieusement** l'information. Pour l'ASCII courant et les caractères
échappés usuels, le couple reste fidèle.

### Le socle commun

- **Aucun type valeur.** Il n'existe pas de DOM JSON dans ces headers : la donnée vit dans un
  `NkArchive` que **vous** possédez ; Reader et Writer ne sont que des convertisseurs sans état.
- **Ownership / RAII.** `NkArchive` et `NkString` sont fournis et possédés par l'appelant dans
  **toutes** les API. Les classes Reader/Writer ne possèdent rien (statiques, sans état) — d'où leur
  thread-safety.
- **Symétrie par conception.** Le Reader est fait pour relire le format du Writer ; tous deux
  passent par l'échappement de `NkJSONValue.h`. Pensez-les comme un **couple**, pas comme deux
  outils indépendants.
- **Erreurs sans exception.** Tout signale l'échec par valeur de retour (`nk_bool`) ou par drapeau
  (`nk_bool& ok`), jamais par exception (« noexcept-friendly »).

---

### Exemple récapitulatif

```cpp
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONValue.h"
using namespace nkentseu;

// 1. Écrire un NkArchive en JSON lisible, dans un buffer réutilisable.
NkArchive scene = BuildSceneArchive();
NkString  json;
if (NkJSONWriter::WriteArchive(scene, json, /*pretty*/true, /*indent*/2)) {
    WriteFile("scene.nkscene", json);
}

// 2. Le relire dans une nouvelle archive, avec message d'erreur positionné.
NkArchive reloaded;
NkString  error;
if (!NkJSONReader::ReadArchive(json, reloaded, &error)) {
    NK_LOG_ERROR("Parsing échoué : {}", error);
}

// 3. Échapper / dé-échapper une chaîne brute (la mécanique sous le couple).
NkString esc = NkJSONEscapeString(R"(C:\jeux\"save")");
nk_bool  ok  = false;
NkString raw = NkJSONUnescapeString(esc, ok);   // testez 'ok' avant d'utiliser 'raw'
```

---

[← Index NKSerialization](README.md) · [Récap NKSerialization](../NKSerialization.md) · [Couche System](../README.md)
