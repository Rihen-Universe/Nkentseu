# Le pipeline d'assets

> Couche **System** · NKSerialization · Transformer un fichier source (`.fbx`, `.png`, `.wav`…)
> en un asset moteur autonome `.nkasset` — identité stable, métadonnées, intégrité par CRC —,
> le retrouver via un **registre**, et le **cuisiner** pour une plateforme cible.

Un moteur ne consomme jamais les fichiers que produisent les artistes : un `.fbx` est lourd, lent à
parser, et n'a pas d'identité stable entre deux sessions. Il faut une étape d'**import** qui le
convertit, une fois pour toutes, en un format à nous — compact, versionné, vérifié — et qui lui
attribue un **identifiant** qu'on pourra référencer ailleurs sans craindre qu'il change. C'est tout
le rôle de la famille « Assets » : un format de fichier (`.nkasset`), un **GUID** stable, un sac de
**métadonnées**, un **registre** global pour les retrouver, et deux outils — l'**importeur** (source
→ asset) et le **cuiseur** (asset → build final). La règle d'or, partout : **un asset référencé par
chemin a un ID déterministe** ; deux sessions, deux machines, deux builds donnent le même ID pour le
même chemin logique.

Ce n'est **pas** un gestionnaire de ressources en mémoire (cache, comptage de références, *streaming*) :
il n'existe d'ailleurs **aucune** classe « AssetManager » dans cette couche — `NkAssetManager.h` est un
fichier **vide**. Ce qu'on documente ici est la couche **fichier + registre + import**, pas la couche
runtime de chargement. Et ce n'est **pas** un sérialiseur générique : le payload est écrit **brut**,
les métadonnées en format **NkNative** (voir [Native](Native.md)) ; on s'appuie sur [`NkArchive`](Archive.md)
pour les propriétés custom et les sous-structures.

- **Namespace** : `nkentseu` (pas de sous-namespace `asset` ; les helpers de format restent dans `nkentseu::native`)
- **Headers** : `#include "NKSerialization/Asset/NkAssetMetadata.h"` (base) · `#include "NKSerialization/Asset/NkAssetImporter.h"` (importeur, inclut le précédent)
- **Pas de header parapluie** dans `Asset/`, et **pas de** `NkAssetManager` (header vide).

> **Note Win32.** `NkAssetMetadata.h` fait `#undef GetObject` après ses includes : `<windows.h>`
> définit une macro `GetObject`/`GetObjectA` qui, sinon, masque `NkArchive::GetObject` et casse le
> link. Si vous incluez Windows en amont, ce `#undef` est ce qui sauve la compilation.

---

## L'identité : `NkAssetId` et `NkAssetPath`

Avant tout, un asset a besoin d'un **nom interne** que rien ne casse. `NkAssetId` est un **GUID
128-bit** (deux `nk_uint64`, `lo` et `hi`). Le point capital est la façon de le **fabriquer**, et il
y a deux mondes qu'il ne faut jamais confondre.

`NkAssetId::FromName(chemin)` produit un ID **déterministe** : c'est un double FNV-1a 64-bit (un
hash par moitié, avec des graines décorrélées pour que `lo` et `hi` ne bougent pas ensemble). Mêmes
caractères en entrée → même ID, aujourd'hui, demain, sur une autre machine. **C'est la seule façon
correcte d'identifier un asset référencé par chemin** : un matériau qui pointe vers
`/Game/Textures/Brick` retrouvera toujours la bonne texture. À l'opposé, `NkAssetId::Generate()` tire
un GUID **pseudo-aléatoire** (un xorshift sur un compteur `static` mélangé à des adresses de pile) :
non déterministe, et **non thread-safe** (le compteur est muté sans verrou — *data race* si plusieurs
threads génèrent en même temps). Réservez-le aux assets **transitoires** sans nom stable.

```cpp
NkAssetId stable = NkAssetId::FromName("/Game/Meshes/Cube");  // même ID partout, toujours
NkAssetId temp   = NkAssetId::Generate();                     // jetable, non déterministe
NkString  s      = stable.ToString();                         // "{HHHHHHHHHHHHHHHH-LLLLLLLLLLLLLLLL}"
NkAssetId back   = NkAssetId::FromString(s);                  // round-trip exact
```

`NkAssetPath` est le **chemin logique** de l'asset dans l'arbre du projet, façon `/Game/Meshes/Cube`.
Il stocke le chemin complet (`path`) **et** le dernier segment (`name`, extrait au constructeur). Il
sait remonter au parent (`GetParent`) et, surtout, traduire le chemin logique en **chemin physique**
sur disque via `ToFilePath(rootDir, ext)` : il retire le premier segment (`/Game`), recolle le reste
sous `rootDir` et ajoute l'extension (`.nkasset` par défaut). C'est le pont entre « comment je nomme
l'asset » et « où il est rangé ».

> **En résumé.** `NkAssetId` = GUID 128-bit. **`FromName(chemin)` = déterministe et stable** → pour
> tout ce qu'on référence ; `Generate()` = aléatoire, non thread-safe → transitoires seulement.
> `NkAssetPath` = chemin logique `/Game/...`, avec `name` extrait et `ToFilePath` pour descendre sur disque.

---

## Les métadonnées : `NkAssetMetadata`

Un asset, c'est son contenu (le *payload*) **plus** tout ce qu'on sait de lui. `NkAssetMetadata`
rassemble cette deuxième partie : son identité (`id`, `assetPath`, `type`, le nom de classe C++
`typeName`, le fichier source d'origine `sourceFilePath`), son **versioning** (`assetVersion`,
`engineVersion`, `importTimestamp` Unix), la **localisation du payload** dans le fichier
(`payloadOffset`, `payloadSize`, `payloadCRC`), et tout ce qui décrit l'asset au-delà : des **tags**
(`NkVector<NkString>`), des **propriétés** custom dans un [`NkArchive`](Archive.md), des
**dépendances** vers d'autres assets, et une **miniature** PNG optionnelle.

Le `type` est un `NkAssetType` (énumération `nk_uint16` : `StaticMesh`, `Texture2D`, `Material`,
`Sound`, `Animation`, `Shader`…). C'est lui qui dit au moteur **comment** lire le payload. Les
**dépendances** (`NkAssetDependency`) sont le mécanisme de graphe : chacune porte l'ID et le chemin
de l'asset visé, plus un drapeau `hardRef` — *hard* signifie « doit être chargé avant moi » (un mesh
a besoin de son matériau), *soft* signifie « je le chargerai paresseusement ».

```cpp
NkAssetMetadata meta;
meta.id   = NkAssetId::FromName("/Game/Meshes/Cube");
meta.type = NkAssetType::StaticMesh;
meta.AddTag("environment");
meta.AddDependency(NkAssetId::FromName("/Game/Materials/Stone"),
                   "/Game/Materials/Stone", /*hard*/ true);
```

`AddTag` et `AddDependency` sont **anti-doublon** (par comparaison de chaîne pour le tag, par ID pour
la dépendance). Toute la structure sait se **sérialiser** : `Serialize(archive)` écrit chaque champ
sous des clés réservées (`__id__`, `__path__`, `__type__`…), avec des sous-archives pour les tags
(`__tags__`), les propriétés (`__props__`, omises si vides) et les dépendances (`__deps__`) ;
`Deserialize` fait l'inverse, en *best-effort* (toute lecture manquante est ignorée). Les deux
renvoient toujours `true` — l'échec n'est pas signalé à ce niveau.

> **En résumé.** `NkAssetMetadata` = tout ce qu'on sait d'un asset *sauf* son contenu : identité,
> versions, position du payload, tags, propriétés (`NkArchive`), dépendances (hard/soft), miniature.
> `AddTag`/`AddDependency` dé-doublonnent ; `Serialize`/`Deserialize` font le pont avec NkNative.

---

## Le fichier `.nkasset` : `NkAssetFileHeader` et `NkAssetIO`

Sur disque, un asset est trois blocs collés : **`[Header (40 octets)][Métadonnées NkNative][Payload]`**.
L'en-tête `NkAssetFileHeader` commence par le magic `"NKASSET0"` (8 octets), suivi de la version de
format, de la taille du bloc de métadonnées, de la position et de la taille du payload, et de **deux**
CRC32 — un pour le payload, un pour l'en-tête lui-même. La structure fait **exactement 40 octets**
(un `static_assert` le garantit ; le commentaire « 32 bytes » dans le code est faux). Pendant le
calcul du `headerCRC`, ce champ est mis à zéro pour que le CRC soit reproductible.

`NkAssetIO` est la porte d'entrée/sortie, **tout en statique** :

- `Write(...)` sérialise les métadonnées, les encode en **NkNative**, calcule le CRC du payload et
  celui de l'en-tête, puis écrit le tout via [`NkFile`](../NKFileSystem/README.md) en binaire. Une
  subtilité : l'encodage se fait en **deux passes** — on doit injecter les valeurs finales
  d'offset/taille/CRC du payload *dans* l'archive avant de la ré-encoder, puisque ces champs en font
  partie.
- `ReadMetadata(...)` lit l'en-tête, valide le magic **et** le `headerCRC`, puis décode les
  métadonnées — **sans toucher au payload**. C'est l'appel léger : un éditeur peut scanner un dossier
  entier en ne lisant que les en-têtes.
- `ReadPayload(...)` saute à `payloadOffset`, lit `payloadSize` octets, et vérifie le `payloadCRC`
  s'il est non nul (mais **pas** le headerCRC ici). Un payload vide donne un succès et un buffer vidé.
- `ReadFull(...)` enchaîne les deux (et rouvre le fichier deux fois).

```cpp
NkAssetMetadata meta;
NkVector<nk_uint8> payload;
if (NkAssetIO::ReadFull("Content/Meshes/Cube.nkasset", meta, payload)) {
    // meta.type == StaticMesh ; payload = octets bruts du mesh, CRC vérifié
}
```

> **En résumé.** Format `.nkasset` = `[Header 40o][Metadata NkNative][Payload]`, magic `"NKASSET0"`,
> **double CRC32** (header + payload). `NkAssetIO` (statique) : `Write` (deux passes pour les champs
> payload), `ReadMetadata` (léger, valide headerCRC), `ReadPayload` (valide payloadCRC), `ReadFull`.

---

## Le registre : `NkAssetRecord` et `NkAssetRegistry`

Pour retrouver un asset sans ouvrir tous les fichiers, on tient un **registre** : un index léger qui,
pour chaque asset, garde juste de quoi le localiser — son ID, son chemin logique, son type, son nom de
classe et le **chemin disque** du `.nkasset` (`NkAssetRecord`). Pas de payload, pas de tags : c'est
volontairement maigre.

`NkAssetRegistry::Global()` donne le registre **singleton** (un *Meyers singleton*). On l'alimente
soit en lisant un fichier (`RegisterAsset(diskPath)` lit ses métadonnées et met à jour ou crée le
record), soit directement (`Register(rec)`). On interroge par `FindById`, `FindByPath`, `FindByName`,
ou en masse par `GetByType`. Tout cela est implémenté en **recherche linéaire** sur un
`NkVector<NkAssetRecord>` — il n'y a **pas** d'index hashé : `O(n)` par requête, parfait pour quelques
milliers d'assets, à surveiller au-delà. Le registre se **sauve** et se **recharge**
(`SaveRegistry`/`LoadRegistry`, en NkNative), ce qui évite de re-scanner tout le contenu au démarrage.

```cpp
auto& reg = NkAssetRegistry::Global();
reg.RegisterAsset("Content/Meshes/Cube.nkasset");
if (const NkAssetRecord* r = reg.FindByName("Cube"))
    NkAssetIO::ReadFull(r->diskPath.Cstr(), meta, payload);
```

Deux mises en garde fermes. **`Global()` n'est pas thread-safe** au-delà de l'initialisation : les
mutations de la liste interne se font sans verrou — sérialisez les accès côté appelant. Et
**`GetByTag` est un STUB** : son corps est quasi vide, il n'écrit **rien** dans la sortie (les tags ne
sont pas dans le record léger). N'en attendez rien tant qu'il n'est pas implémenté côté projet.

> **En résumé.** `NkAssetRecord` = entrée légère (id/path/type/typeName/diskPath, sans payload).
> `NkAssetRegistry::Global()` = singleton, recherche **linéaire** (`FindById/ByPath/ByName`,
> `GetByType`), persistance `SaveRegistry`/`LoadRegistry`. **Non thread-safe**. **`GetByTag` ne fait rien.**

---

## L'import et la cuisson : `NkAssetImporter` et `NkAssetCooker`

`NkAssetImporter::Import(source, outputDir, opts)` est l'opération qui démarre le pipeline : elle lit
le fichier source **brut** (le payload n'est pas transformé), **détecte le type** par extension,
calcule le nom et le chemin logique, attribue `id = FromName(cheminLogique)` (donc **stable** : ré-importer
le même asset donne le même ID), pose le timestamp, applique les tags automatiques, écrit le
`.nkasset` via `NkAssetIO::Write`, et **enregistre** le résultat dans `NkAssetRegistry::Global()`.
Le résultat (`NkImportResult`) dit si ça a marché, l'ID produit, le chemin de sortie, et les tailles.
Les options (`NkImportOptions`) couvrent le chemin cible, un nom forcé, des tags et des propriétés
custom — mais **attention**, `compressPayload` et `generateThumbnail` sont déclarés et **inertes** (la
compression LZ4 et la génération de miniature ne sont pas implémentées).

`DetectType` est le petit dictionnaire qui mappe les extensions vers `NkAssetType` (`fbx/obj/gltf` →
mesh, `png/jpg/tga/hdr/exr/dds` → texture, `wav/ogg/mp3` → son, etc.), en **ignorant la casse**. Sur
cette base, deux opérations dérivées : `Reimport` relit les métadonnées d'un `.nkasset` existant et
relance `Import` sur sa source (en échouant si l'asset n'a pas de source — un asset « manuel ») ; et
`NeedsReimport` compare la date de modification du fichier source (`stat`) au `importTimestamp` stocké,
et répond `true` si la source est plus récente.

`NkAssetCooker::Cook` produit la version **build final** : il relit l'asset, **dépouille** ce qui ne
sert qu'à l'éditeur (`stripSourcePaths` efface le chemin source, `stripThumbnails` la miniature),
ajoute un tag de plateforme (`"PC"`/`"console"`/`"mobile"`/`"web"`) plus un tag `"cooked"`, et
réécrit le `.nkasset` dans le dossier de sortie. Là encore, deux options sont **inertes** :
`compressPayload` et `maxTextureSize` sont déclarées dans `CookOptions` mais non utilisées.

```cpp
NkImportOptions opts;
opts.targetPath = "/Game/Textures";
opts.autoTags.PushBack("ui");
NkImportResult r = NkAssetImporter::Import("Raw/icon.png", "Content/", opts);
// r.assetId == FromName("/Game/Textures/icon") — stable au ré-import

NkAssetCooker::CookOptions cook; cook.platform = NkAssetCooker::Platform::Mobile;
NkAssetCooker::Cook("Content/icon.nkasset", "Build/Mobile/", cook);
```

> **En résumé.** `Import` : source brute → `.nkasset` + enregistrement auto, **ID stable via FromName**.
> `DetectType` par extension. `Reimport`/`NeedsReimport` pour le suivi des sources. `Cook` dépouille
> pour le build et tague la plateforme. **Inertes** : `compressPayload`, `generateThumbnail`, `maxTextureSize`.

---

## Aperçu de l'API

Tous les éléments publics de la famille « Assets », répartis par fichier. Complexités entre crochets.

### `NkAssetMetadata.h` — identité et métadonnées

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| GUID | `NkAssetId{ lo, hi }` | Identifiant 128-bit (deux `nk_uint64`). |
| GUID — état | `IsValid()` `[O(1)]`, `IsNull()` `[O(1)]` | Non-nul ? / nul ? |
| GUID — compar. | `operator== != <` `[O(1)]` | Égalité, ordre lexicographique `(hi, lo)` (conteneurs triés). |
| GUID — fabrique | `static Generate()` `[O(1)]` | GUID aléatoire **non déterministe, non thread-safe** (transitoires). |
| GUID — fabrique | `static FromName(name)` `[O(n)]` | GUID **déterministe** (double FNV-1a) — pour référence par chemin. |
| GUID — fabrique | `static Invalid()` `[O(1)]` | Référence vers le singleton nul. |
| GUID — texte | `ToString()` `[O(1)]`, `static FromString(s)` `[O(1)]` | `"{HHHH-LLLL}"` ↔ ID (nul si `s.Length() < 35`). |
| Chemin | `NkAssetPath()`, `explicit NkAssetPath(p)` | Chemin logique ; extrait `name` du dernier segment. |
| Chemin — données | `.path` `.name` | Chemin complet / dernier segment. |
| Chemin | `IsValid()`, `operator==` (sur `path`), `GetParent()`, `ToFilePath(root, ext=".nkasset")` | Valide ? / égalité / parent / chemin physique. |
| Type | `enum class NkAssetType : nk_uint16` | `Unknown=0`, `StaticMesh`, `SkeletalMesh`, `Texture2D/Cube`, `Material(Instance)`, `Sound`, `Animation`, `Blueprint`, `DataTable`, `Map`, `World`, `Prefab`, `Font`, `Shader`, `Script`, `Custom=255`. |
| Type | `NkAssetTypeName(t)` `[O(1)]` | Nom littéral du type (`Unknown` par défaut). |
| Dépendance | `NkAssetDependency{ id, path, hardRef=true }` | Lien vers un autre asset (hard = bloquant, soft = lazy). |
| Métadonnées | `NkAssetMetadata` (champs id/path/type/typeName/source, versions, payload, tags, properties, deps, thumbnail) | Tout sauf le contenu. |
| Métadonnées | `AddTag(t)` `[O(n)]`, `HasTag(t)` `[O(n)]` | Ajout anti-doublon / test. |
| Métadonnées | `AddDependency(id, path, hard=true)` `[O(n)]` | Dépendance anti-doublon (par id). |
| Métadonnées | `Serialize(ar)` / `Deserialize(ar)` `[O(n)]` | NkArchive ↔ métadonnées (toujours `true`). |
| En-tête | `NkAssetFileHeader` (magic `"NKASSET0"`, version, flags, metadataSize, payload off/size/CRC, headerCRC) | En-tête **fixe 40 octets** (`static_assert`). |
| IO | `static NkAssetIO::Write(path, meta, payload, size, err)` `[O(taille)]` | Écrit `[Header][Meta NkNative][Payload]` + double CRC. |
| IO | `static ReadMetadata(path, out, err)` `[O(meta)]` | Lit en-tête + meta, valide headerCRC, **pas le payload**. |
| IO | `static ReadPayload(path, out, err)` `[O(payload)]` | Lit le payload, valide payloadCRC (si non nul). |
| IO | `static ReadFull(path, meta, payload, err)` `[O(taille)]` | Métadonnées + payload (ouvre 2×). |
| Record | `NkAssetRecord{ id, assetPath, type, typeName, diskPath, loaded }` | Entrée légère du registre (sans payload). |
| Registre | `static NkAssetRegistry::Global()` `[O(1)]` | Singleton (**non thread-safe** en mutation). |
| Registre | `RegisterAsset(diskPath, err)` `[O(n)]`, `Register(rec)` `[O(n)]`, `Unregister(id)` `[O(n)]` | Inscrire (par fichier / direct) / retirer. |
| Registre | `FindById/ByPath/ByName` `[O(n)]`, `GetByType(t, out)` `[O(n)]` | Recherche **linéaire**. |
| Registre | `GetByTag(tag, out)` | **STUB** — n'écrit rien. |
| Registre | `Count()` `[O(1)]`, `Records()` `[O(1)]`, `Clear()` `[O(n)]` | Compte / accès direct / vidage. |
| Registre | `SaveRegistry(path, err)` / `LoadRegistry(path, err)` `[O(n)]` | Persistance NkNative (sans `loaded`/`name`). |

### `NkAssetImporter.h` — import et cuisson

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Options import | `NkImportOptions{ compressPayload*, generateThumbnail*, targetPath, overrideName, autoTags, extraProperties }` | Réglages d'import (`*` = **inerte**). |
| Résultat import | `NkImportResult{ success, assetId, outputPath, error, payloadSize, outputSize }` | Issue de l'import. |
| Import | `static NkAssetImporter::DetectType(src)` `[O(ext)]` | Type par extension (case-insensitive). |
| Import | `static Import(src, outDir, opts={})` `[O(taille)]` | Source brute → `.nkasset` + enregistre dans `Global()`, **id = FromName**. |
| Import | `static Reimport(assetPath, opts={})` `[O(taille)]` | Relit meta, relance `Import` sur la source (échec si pas de source). |
| Import | `static NeedsReimport(assetPath)` `[O(IO)]` | Source plus récente que `importTimestamp` ? |
| Cuisson | `enum class NkAssetCooker::Platform { PC, Console, Mobile, Web }` | Cible de build. |
| Cuisson | `NkAssetCooker::CookOptions{ platform, compressPayload*, stripSourcePaths, stripThumbnails, maxTextureSize* }` | Réglages de cuisson (`*` = **inerte**). |
| Cuisson | `static Cook(inAsset, outDir, opts, err)` `[O(taille)]` | Dépouille + tag plateforme/`"cooked"`, réécrit le `.nkasset`. |
| (vide) | `NkAssetManager.h` | **Fichier vide (0 octet)** — aucune classe `NkAssetManager`. |

---

## Référence complète

Chaque élément repris en détail. Le trivial est bref ; ce qui porte un piège ou un choix de
conception est traité **à fond**, avec ses usages dans les différents domaines du moteur.

### `NkAssetId` — l'identité stable

Un GUID 128-bit (`lo`, `hi`). `IsValid()`/`IsNull()`, l'égalité et l'ordre (`<` lexicographique sur
`(hi, lo)`, qui permet de ranger des IDs dans un conteneur trié) sont triviaux et `O(1)`. Tout
l'enjeu est dans les deux fabriques.

`FromName(name)` est la pierre angulaire de tout le système : un **double FNV-1a 64-bit** (deux
graines distinctes, deux nombres premiers décorrélés pour que `lo` et `hi` ne hashent pas à
l'identique ; `lo` forcé à 1 s'il tombe à zéro). Mêmes octets en entrée → même ID, **cross-session et
cross-machine**. Usages par domaine :

- **Outils / éditeur** — référencer un asset par son chemin logique (`/Game/Materials/Stone`) dans un
  `.nkproj` ou un inspecteur, sans stocker un GUID volatil : il se recalcule à la volée.
- **Rendu / ECS / animation** — un composant *MeshRenderer* qui pointe vers un mesh, un *AnimGraph*
  vers un clip : on sérialise l'ID `FromName`, jamais un pointeur, et il reste valide après un build.
- **IO / réseau** — synchroniser quel asset les pairs chargent : l'ID déterministe est la même clé des
  deux côtés du fil, sans table de correspondance.

`Generate()` tire un GUID **aléatoire** (xorshift sur un compteur `static` + adresses de pile). Deux
pièges à graver : il est **non déterministe** (inutilisable pour une référence persistante) et **non
thread-safe** (le `static` muté sans verrou = *data race*). Réservé aux assets **transitoires**
(résultats de génération procédurale en jeu, scratch). `ToString()`/`FromString()` font le round-trip
texte (`"{HHHH...-LLLL...}"`, `hi` d'abord) ; `FromString` rend l'ID nul si la chaîne fait moins de
35 caractères. `Invalid()` renvoie une référence vers un singleton nul, pratique comme valeur sentinelle.

### `NkAssetPath` — le chemin logique

Le couple chemin complet + nom. Le constructeur explicite extrait `name` du dernier segment (après
`/` ou `\`). `GetParent()` remonte d'un cran (`"/"` si pas de slash). La méthode qui compte est
`ToFilePath(rootDir, ext)` : elle **retire le premier segment** logique (le `/Game` de
`/Game/Meshes/Cube`), recolle le reste sous `rootDir` et ajoute l'extension — c'est la traduction
« nom dans le projet » → « fichier sur disque ». Usages : l'**éditeur** affiche `/Game/...` à
l'utilisateur mais lit `Content/...nkasset` ; le **build** remappe `rootDir` pour pointer vers le
dossier cuisiné. `operator==` ne compare que `path` (le `name` étant dérivé), donc deux chemins égaux
sont égaux quel que soit le `name`.

### `NkAssetType` et `NkAssetTypeName` — le quoi

L'énumération `nk_uint16` énumère ce que le moteur sait charger (`StaticMesh`, `SkeletalMesh`,
`Texture2D`, `TextureCube`, `Material`, `MaterialInstance`, `Sound`, `Animation`, `Blueprint`,
`DataTable`, `Map`, `World`, `Prefab`, `Font`, `Shader`, `Script`, `Custom=255`). C'est le
**discriminant** qui dit comment interpréter le payload :

- **Rendu** — `StaticMesh`/`SkeletalMesh`/`Texture2D`/`TextureCube`/`Material`/`Shader` couvrent le
  gros du pipeline graphique ; le type oriente vers le bon désérialiseur de payload.
- **Audio** — `Sound` route le payload vers le décodeur audio.
- **Gameplay / outils** — `Blueprint`, `DataTable`, `Prefab`, `World`, `Map`, `Script` portent la
  logique et le contenu de niveau. `Custom` est l'échappatoire pour les types maison.

`NkAssetTypeName(t)` rend le littéral correspondant (`"Unknown"` par défaut) — utile pour les logs,
l'éditeur, les tags automatiques.

### `NkAssetDependency` et le graphe — l'ordre de chargement

Chaque dépendance porte l'`id`, le `path` de l'asset visé et `hardRef`. Une **référence dure** (un
mesh → son matériau, un matériau → ses textures) doit être résolue **avant** : c'est ce qui permet de
charger un graphe dans le bon ordre, ou d'inclure les bons assets dans un build. Une **référence
molle** (`hardRef=false`) signale un lien paresseux qu'on chargera à la demande (un niveau référant le
suivant, un asset volumineux chargé seulement quand on en approche). En **outils**, ce graphe alimente
la cuisson (transitivité : ce dont a besoin un asset entre dans le build) et la détection des assets
orphelins.

### `NkAssetMetadata` — le sac de données

Tous les champs sont publics et triviaux d'accès ; ce qui mérite attention, c'est la **sérialisation**
sous clés réservées. `Serialize` écrit l'identité, les versions et les pointeurs payload en clés
`__...__`, puis des sous-archives pour les tags (`__tags__`, chaque entrée `tag_<i>` + un `count`),
les propriétés custom (`__props__`, **omises si vides**) et les dépendances (`__deps__`, chaque
`dep_<i>` portant `id`/`path`/`hard`). `Deserialize` relit tout en *best-effort* (chaque lecture
manquante est silencieusement ignorée, d'où le `(void)` sur les `Get*`). Les deux renvoient toujours
`true`. `AddTag`/`HasTag` (linéaires sur les tags) et `AddDependency` (linéaire, dé-doublonne par id)
sont les seules méthodes « actives ». Usages : un **éditeur** ajoute des tags (`"ui"`, `"hero"`) pour
filtrer le browser ; un **système de build** lit les dépendances ; le champ `properties` (un
[`NkArchive`](Archive.md)) accueille tout réglage custom par type d'asset.

### `NkAssetFileHeader` — l'en-tête fixe

40 octets exactement (`static_assert` — le commentaire « 32 bytes » est **erroné**). Le magic
`"NKASSET0"` permet de reconnaître un `.nkasset` au premier coup d'œil. Les champs payload
(offset/size/CRC) localisent et vérifient le contenu sans le lire ; `headerCRC` protège l'en-tête
lui-même (mis à zéro pendant son propre calcul). `flags` est réservé. Cette taille fixe est ce qui
permet à `ReadMetadata` de ne lire que 40 octets pour décider de la suite.

### `NkAssetIO` — l'entrée/sortie du format

Quatre fonctions statiques, toutes `noexcept`, toutes par [`NkFile`](../NKFileSystem/README.md).

- `Write` — la seule subtilité est le **double encodage** : les champs payload (offset/taille/CRC)
  font partie de l'archive de métadonnées, donc on encode une première fois pour connaître la taille,
  on **réinjecte** les valeurs finales, on ré-encode, puis on écrit `[Header][Meta][Payload]` avec les
  deux CRC. Le paramètre `err` (optionnel) reçoit un message en cas d'échec.
- `ReadMetadata` — le chemin **léger** : lit l'en-tête (40 o), valide le magic **et** le headerCRC
  (recalcul avec le champ à 0), décode les métadonnées, appelle `Deserialize`. **Ne touche pas au
  payload**. C'est l'appel d'un **scanner d'éditeur** qui veut lister un dossier sans tout charger.
- `ReadPayload` — saute à `payloadOffset`, lit `payloadSize` octets, vérifie le payloadCRC s'il est
  non nul (mais **pas** le headerCRC ici). Payload vide → buffer vidé + succès. C'est l'appel du
  **chargeur runtime** d'un domaine donné (le mesh, la texture, le son lit ses octets bruts).
- `ReadFull` — `ReadMetadata` puis `ReadPayload` (ouvre le fichier **deux fois**) : pratique quand on
  veut tout, sans optimiser les ouvertures.

L'intégrité repose sur **CRC32** (`native::NkCRC32`) à deux niveaux, ce qui détecte un fichier tronqué
ou corrompu avant que le moteur ne consomme des octets faux.

### `NkAssetRecord` et `NkAssetRegistry` — retrouver les assets

Le `NkAssetRecord` est délibérément **léger** (id/path/type/typeName/diskPath/loaded) : c'est un index,
pas un cache. `NkAssetRegistry::Global()` (singleton Meyers) tient ces records dans un `NkVector`, et
**tout est recherche linéaire** — il n'y a aucun index hashé. `RegisterAsset(diskPath)` lit les
métadonnées du fichier et met à jour (par id) ou crée un record ; `Register(rec)` insère/remplace sans
fichier ; `Unregister(id)` retire le premier match. Côté lecture : `FindById`/`FindByPath`/`FindByName`
rendent un pointeur ou `nullptr`, `GetByType` collecte tous les records d'un type. Usages par domaine :

- **Outils / éditeur** — le *content browser* parcourt `Records()`, filtre par type (`GetByType`),
  cherche par nom — `O(n)` suffit largement pour quelques milliers d'assets.
- **Chargement (tous domaines)** — on résout une référence (`FindById`), on récupère `diskPath`, on
  lit le payload via `NkAssetIO`. Le registre est l'annuaire ; le chargement effectif vit ailleurs.
- **Démarrage / IO** — `SaveRegistry`/`LoadRegistry` (NkNative) évitent de re-scanner tout le contenu
  à chaque lancement (note : `loaded` et `assetPath.name` ne sont **pas** persistés).

Deux pièges fermes : `Global()` n'est **pas thread-safe** en mutation (sérialiser les accès), et
**`GetByTag` est un no-op** — il n'écrit rien (les tags ne sont pas dans le record léger) ; ne comptez
pas dessus tant qu'il n'est pas implémenté côté projet.

### `NkAssetImporter` — la source devient asset

`DetectType(src)` est un dictionnaire d'extensions → type (case-insensitive) : meshes (`fbx/obj/gltf/
glb/dae`), textures (`png/jpg/jpeg/tga/bmp/hdr/exr/dds`), sons (`wav/ogg/mp3/flac`), shaders (`glsl/
hlsl/vert/frag/comp/spv`), scripts (`lua/py/js`), fonts (`ttf/otf`), data (`json/csv/xml/yaml`) ; pas
de point → `Unknown`, extension inconnue → `Custom`.

`Import(src, outDir, opts)` orchestre tout : lit la source **brute** (payload non transformé), détecte
le type, calcule le nom (`overrideName` ou basename sans extension) et le chemin logique
(`opts.targetPath` + nom, sinon `/Game/<name>`), pose `meta.id = FromName(cheminLogique)` (donc
**stable au ré-import**), tamponne le timestamp (`time(nullptr)`), ajoute les `autoTags` plus un tag du
nom de type, fusionne `extraProperties` et y injecte `source_format`/`source_size`, écrit
`<outDir>/<name>.nkasset` via `NkAssetIO::Write`, et **enregistre** dans `NkAssetRegistry::Global()`.
La taille de sortie est mesurée via `fopen/fseek/ftell` (cstdio CRT — aucun buffer alloué à libérer
côté appelant). `NkImportResult` rapporte le succès, l'ID, le chemin et les tailles.

- **Outils / pipeline de contenu** — le cas central : déposer un `.fbx` ou un `.png` produit un
  `.nkasset` prêt et référencé, avec un ID stable réutilisable partout.
- **Itération artiste** — `Reimport(assetPath)` relit les métadonnées et relance `Import` sur la
  source (échec si l'asset n'a **pas** de source — un asset « manuel » créé dans l'éditeur). `targetPath`
  et `overrideName` sont repris de l'existant si non fournis.
- **Build incrémental** — `NeedsReimport(assetPath)` compare le `st_mtime` (via `stat`) du source au
  `importTimestamp` : un système de build ne ré-importe que ce qui a changé.

Restent **inertes** : `NkImportOptions::compressPayload` (LZ4 non implémenté) et `generateThumbnail`
(pas de lib image branchée). Ne les présentez pas comme fonctionnels.

### `NkAssetCooker` — la cuisson pour le build

`Cook(inAsset, outDir, opts)` produit la version distribuée : `ReadFull`, puis dépouillement de ce qui
ne sert qu'à l'édition — `stripSourcePaths` efface `sourceFilePath`, `stripThumbnails` vide
`thumbnailData` —, ajout d'un tag de plateforme (`"PC"`/`"console"`/`"mobile"`/`"web"` selon
`Platform`) plus un tag `"cooked"`, et réécriture du `.nkasset` dans `outDir`. Usages : **outils de
build** (un passage par plateforme cible, packaging final), **CI** (cuisson automatisée). Deux options
de `CookOptions` sont déclarées mais **inertes** : `compressPayload` et `maxTextureSize` ne sont pas
utilisées par `Cook`.

### `NkAssetManager.h` — le fichier vide

À documenter pour lever toute ambiguïté : **ce header est vide (0 octet)**. Il ne déclare aucun
symbole, et il **n'existe aucune classe `NkAssetManager`** dans cette couche. La gestion runtime des
ressources (cache, comptage de références, *streaming*) n'appartient pas à NKSerialization. N'inventez
rien autour de ce nom.

### Le socle commun

- **Allocation / RAII.** Tous les buffers passent par `NkVector<nk_uint8>` (allocateur NKMemory) et
  `NkString` — aucune gestion mémoire manuelle exposée à l'appelant. Seule exception : `Import` utilise
  `fopen/fseek/ftell/fclose` (cstdio) **uniquement** pour mesurer `outputSize`, sans buffer CRT à libérer.
- **Format de stockage.** Les métadonnées et le registre sont encodés en **NkNative** (voir
  [Native](Native.md)) ; les propriétés custom passent par [`NkArchive`](Archive.md) ; l'IO fichier
  par [`NkFile`](../NKFileSystem/README.md).
- **Intégrité.** Double **CRC32** (en-tête + payload) ; `ReadMetadata` valide le headerCRC,
  `ReadPayload` le payloadCRC (si non nul) — mais pas l'inverse.
- **Threading.** `NkAssetRegistry::Global()` et `NkAssetId::Generate()` portent un état global mutable
  **sans verrou** : sérialisez les accès côté appelant.
- **Stubs / inertes — à ne pas documenter comme fonctionnels.** `NkAssetRegistry::GetByTag` (no-op),
  `NkImportOptions::compressPayload`/`generateThumbnail`, `NkAssetCooker::CookOptions::compressPayload`/
  `maxTextureSize`, et `NkAssetManager.h` (vide).
- **Piège Win32.** `#undef GetObject` en tête de `NkAssetMetadata.h` est requis si `<windows.h>` est
  inclus en amont, sans quoi `NkArchive::GetObject` ne lie pas.

---

### Exemple

```cpp
#include "NKSerialization/Asset/NkAssetImporter.h"   // inclut NkAssetMetadata.h
using namespace nkentseu;

// 1) Import : un fichier source brut devient un .nkasset référencé.
NkImportOptions opts;
opts.targetPath = "/Game/Textures";
opts.autoTags.PushBack("ui");
NkImportResult r = NkAssetImporter::Import("Raw/icon.png", "Content/", opts);
// r.assetId == NkAssetId::FromName("/Game/Textures/icon") — stable d'une session à l'autre

// 2) Registre : retrouver l'asset par son nom logique.
auto& reg = NkAssetRegistry::Global();
if (const NkAssetRecord* rec = reg.FindByName("icon")) {
    // 3) Lecture : métadonnées légères, puis payload vérifié par CRC.
    NkAssetMetadata meta;
    NkVector<nk_uint8> payload;
    if (NkAssetIO::ReadFull(rec->diskPath.Cstr(), meta, payload)) {
        // meta.type == NkAssetType::Texture2D ; payload = octets PNG bruts
    }
}

// 4) Build : cuire l'asset pour une plateforme cible (dépouille + tague).
NkAssetCooker::CookOptions cook;
cook.platform = NkAssetCooker::Platform::Mobile;
NkAssetCooker::Cook("Content/icon.nkasset", "Build/Mobile/", cook);
```

---

[← Index NKSerialization](README.md) · [Récap NKSerialization](../NKSerialization.md) · [Couche System](../README.md)
