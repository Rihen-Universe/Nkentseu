# Le cœur de la sérialisation

> Couche **System** · NKSerialization · Transformer un objet vivant en **données portables**
> qu'on peut écrire, relire, faire migrer entre versions, puis ré-instancier : le modèle de
> document `NkArchive`, l'interface `NkISerializable`, les registres de types, le versionnement
> de schéma, et la façade `NkSerializer` qui relie tout cela aux formats JSON / XML / YAML /
> binaire / natif.

Tôt ou tard, **tout état utile doit quitter la mémoire vive** : sauvegarder une partie,
recharger une scène, envoyer une entité sur le réseau, écrire un fichier de configuration,
exporter un asset depuis l'éditeur. À ce moment-là, la structure C++ en mémoire — avec ses
pointeurs, son agencement précis, son alignement dépendant du compilateur — devient
inutilisable telle quelle : elle n'a de sens que pour *ce* programme, *cette* compilation,
*cette* machine. La sérialisation est l'art de la traduire en une représentation **neutre,
explicite et reconstructible**, et la désérialisation l'opération inverse. Toute la difficulté
tient en une phrase : **il faut un format intermédiaire assez riche pour porter n'importe quel
objet, assez stable pour survivre aux changements de code, et assez neutre pour traverser les
formats de fichier.** Ce format intermédiaire, ici, c'est `NkArchive`.

NKSerialization sépare donc deux mondes : d'un côté un **modèle de document en mémoire**
(`NkArchive`, une table associative ordonnée de valeurs scalaires, d'objets imbriqués et de
tableaux) qui ne sait rien des fichiers ; de l'autre des **lecteurs/écrivains de format** (JSON,
XML, YAML, binaire, natif) qui ne savent que convertir un `NkArchive` en octets et inversement.
Vos types implémentent l'interface `NkISerializable` pour se peindre dans une archive et se
relire depuis une archive — sans jamais toucher au moindre format. Les **registres**
(`NkSerializableRegistry` par nom, `NkComponentRegistry` par identifiant de type ECS) permettent
de ré-instancier un type à partir de son seul nom, et le **versionnement** (`NkSchemaRegistry`,
migrations) fait survivre les anciennes sauvegardes aux évolutions du schéma.

Le module est **zero-STL** : `NkVector`, `NkString`, `NkStringView`, `NkFunction`,
`NkUniquePtr`, types primitifs `nk_bool`/`nk_int64`/`nk_uint64`/`nk_float64`/`nk_size`.

- **Namespace** : `nkentseu` (les helpers internes vivent dans `nkentseu::detail`)
- **Header parapluie** : `#include "NKSerialization/NkSerializer.h"`

> **Note importante.** Plusieurs commentaires Doxygen des en-têtes montrent une API *aspirationnelle*
> de `NkArchive` (`archive.Write(...)`, `Read(...)`, `OpenForWrite/Close`, `NkOwned<T>`). **Aucune
> de ces méthodes n'existe.** L'API réelle de `NkArchive` est faite de `Set*` / `Get*`. Cette page
> ne documente que le code réellement déclaré.

---

## L'archive : le document portable `NkArchive`

`NkArchive` est la **pièce maîtresse** : une table associative ordonnée qui associe des clés
(`NkStringView`) à des nœuds. Un nœud peut être une **valeur scalaire** (booléen, entier, flottant,
chaîne), un **objet imbriqué** (un autre `NkArchive`), ou un **tableau** de nœuds. C'est, en somme,
un petit DOM neutre — l'équivalent d'un objet JSON, mais indépendant de tout format. On y écrit avec
les `Set*` et on en relit avec les `Get*`.

```cpp
NkArchive arc;
arc.SetString("name", "Goblin");
arc.SetInt32("hp", 30);
arc.SetFloat32("speed", 2.5f);

NkInt32 hp = 0;
arc.GetInt32("hp", hp);          // hp == 30, retourne true si la clé existe et est lisible
```

Le détail qui change tout : une valeur scalaire (`NkArchiveValue`) porte une **double
représentation** — l'union binaire `raw` *et* un **texte canonique** `text`. Les flottants sont
formatés en `%.17g`, c'est-à-dire un *round-trip sans perte* : on relit exactement le même nombre.
Cette duplication est ce qui permet à la même archive d'être écrite aussi bien en JSON (où l'on veut
le texte) qu'en binaire (où l'on veut les octets). Conséquence subtile : `NkArchiveValue::operator==`
compare le **type et le texte canonique** — *pas* le champ `raw`.

Les `Get*` sont **tolérants** : ils coercent entre types numériques et savent parser depuis le texte
(un `int64` lu en `float64`, un texte `"42"` lu en entier). Un `float` lu en entier est **tronqué**.
Chaque `Get*` retourne un `nk_bool` (succès) et écrit dans un paramètre de sortie `out` — il ne
*throw* jamais, il signale.

Ce n'est **pas** une base de données ni un index : la recherche d'une clé est **linéaire `O(n)`**
(optimisée pour `n < 100`). C'est parfait pour décrire un objet, mauvais pour stocker des dizaines de
milliers d'entrées plates. Ce n'est **pas** thread-safe non plus.

Au-delà de l'API plate (clé simple), `NkArchive` offre une API **hiérarchique par chemin** :
`SetPath("player.stats.hp", node)` crée au passage les objets intermédiaires manquants, et
`GetPath("player.stats.hp")` renvoie un pointeur (ou `nullptr` si un maillon manque). Enfin, des
**méta-données** se rangent sous la clé spéciale `"__meta__"` via `SetMeta`/`GetMeta` — c'est là que
le versionnement stocke `schema_version`.

> **En résumé.** `NkArchive` = un DOM neutre clé→nœud (scalaire / objet / tableau), écrit par `Set*`,
> relu par des `Get*` tolérants. Double représentation `raw`+`text` (round-trip flottant `%.17g`).
> Recherche `O(n)`, non thread-safe, copie profonde. API plate **et** hiérarchique (`SetPath`/`GetPath`,
> séparateur `.`), méta sous `"__meta__"`. Ce n'est ni une base de données ni un format de fichier.

---

## Se rendre sérialisable : `NkISerializable`

Pour qu'un type sache se sauvegarder, il implémente l'interface `NkISerializable` : trois méthodes
virtuelles pures. `Serialize(NkArchive&)` se *peint* dans une archive, `Deserialize(const NkArchive&)`
se *relit* depuis une archive, et `GetTypeName()` renvoie son nom (une chaîne **statique**, à durée de
vie globale). Le contrat est simple mais strict : **l'ordre des champs lus dans `Deserialize` doit
correspondre à celui écrit dans `Serialize`**, et le nom doit rester stable.

```cpp
struct Enemy : NkISerializable {
    NkInt32 hp = 0; NkString name;
    nk_bool Serialize(NkArchive& a) const override {
        a.SetInt32("hp", hp); a.SetString("name", name.View()); return true;
    }
    nk_bool Deserialize(const NkArchive& a) override {
        a.GetInt32("hp", hp); a.GetString("name", name); return true;
    }
    const char* GetTypeName() const noexcept override { return "Enemy"; }
};
```

L'intérêt d'une interface polymorphe : on peut sérialiser un objet **sans connaître son type concret**
(via un `NkISerializable*`), et — c'est là tout le sel — le **ré-instancier** plus tard à partir de son
seul nom. C'est ce que permet le registre suivant.

`NkSerializableRegistry` est un **singleton** qui associe un **nom de type** (chaîne) à une *factory*
qui crée une instance neuve. On enregistre un type avec `Register<T>("Nom")` (T doit dériver de
`NkISerializable` et avoir un constructeur par défaut), ou avec `RegisterFactory("Nom", fn)` pour les
types sans constructeur par défaut. Ensuite, `Create("Nom")` rend un `NkISerializable*` fraîchement
alloué. **Attention à l'ownership** : `Create` alloue via `new` brut et c'est à *vous* de faire le
`delete` (la façade `CreateFromArchive`, plus bas, l'enveloppe dans un `NkUniquePtr` pour vous éviter
ce piège). La macro `NK_REGISTER_SERIALIZABLE(Type)`, à placer **uniquement dans un `.cpp`**, automatise
l'enregistrement au démarrage.

> **En résumé.** `NkISerializable` = trois virtuelles pures (`Serialize`/`Deserialize`/`GetTypeName`) ;
> ordre lecture = ordre écriture, nom statique stable. `NkSerializableRegistry` (singleton, par **nom**)
> permet de **ré-instancier** un type à partir de sa chaîne via `Create` — mais `Create` rend un `new`
> brut dont vous êtes responsable. Macro `NK_REGISTER_SERIALIZABLE` dans un `.cpp` seulement.

---

## Sérialiser des composants ECS : `NkComponentRegistry`

`NkSerializableRegistry` indexe par nom ; `NkComponentRegistry` répond à un autre besoin : sérialiser
des **composants ECS** indexés par leur **`NkTypeId`** (un identifiant de type entier, pas une chaîne).
On y enregistre un composant avec `Register<T>("Nom")` — et si l'on ne fournit pas de fonctions de
(dé)sérialisation, le registre en synthétise par défaut : si `T` implémente `NkISerializable`, il
délègue à ses méthodes ; sinon, il fait un **dump hexadécimal brut** des `sizeof(T)` octets sous les
clés `"__raw__"` / `"__rawSz__"`. Ce dump dépanne, mais il n'est **pas portable** entre agencements
mémoire ni entre plateformes — réservez-le au prototypage.

Le registre est entièrement statique et inline. On retrouve un composant par `Find(typeId)`,
`FindByName("Nom")` ou `Find<T>()` ; on (dé)sérialise par `Serialize(id, obj, arc)` /
`Serialize<T>(obj, arc)` et leurs symétriques ; on parcourt tous les types via `ForEach(fn)`. Les
macros `NK_REGISTER_COMPONENT(Type, "Nom")` et `NK_REGISTER_COMPONENT_CUSTOM(Type, "Nom", SerFn, DeserFn)`
enregistrent au chargement.

> **En résumé.** `NkComponentRegistry` = registre ECS par **`NkTypeId`** (distinct de
> `NkSerializableRegistry` par nom). Synthétise des (dé)sérialiseurs par défaut : délégation si
> `NkISerializable`, sinon dump hexa brut **non portable** (`__raw__`/`__rawSz__`). Macros
> `NK_REGISTER_COMPONENT` / `..._CUSTOM`.

---

## Survivre au temps : le versionnement de schéma

Le problème le plus pernicieux de la sérialisation n'est pas d'écrire, c'est de **relire des données
anciennes après que le code a changé**. Vous ajoutez un champ, vous renommez, vous changez une unité —
et toutes les sauvegardes existantes deviennent illisibles ou, pire, lues de travers. NKSerialization
apporte un **système de migration de schéma**.

Chaque type a un `NkTypeId` (obtenu par `NkTypeOf<T>()`, une astuce sans RTTI à la EnTT) et une
**version sémantique** `NkSchemaVersion{major, minor, patch}`, packée en 64 bits. On déclare la version
courante d'un type, puis on enregistre des **migrations** : des fonctions qui transforment une archive
d'une version vers la suivante. Au chargement, `NkSchemaRegistry::MigrateArchive` applique la **chaîne**
de migrations depuis la version stockée jusqu'à la version courante, met à jour
`"__meta__".schema_version`, et s'arrête (garde anti-boucle de **256 itérations**) si aucun chemin
n'existe.

```cpp
// Dans un .cpp :
NK_SCHEMA_CURRENT_VERSION(PlayerSave, 2, 0, 0);
// La 1.0.0 n'avait pas de champ "stamina" : on l'ajoute avec une valeur par défaut.
NK_ADD_FIELD_MIGRATION_FLOAT64(PlayerSave, 1,0,0,  2,0,0,  "stamina", 100.0);
```

Le cas le plus courant — *ajouter un champ avec une valeur par défaut* — est si fréquent qu'il a ses
propres macros (`NK_ADD_FIELD_MIGRATION_BOOL/INT32/FLOAT64/STRING`), bâties sur un adaptateur interne
`NkFieldMigrationAdapter`. Pour une transformation plus complexe, `NK_REGISTER_MIGRATION` enregistre
votre propre fonction. Toutes ces macros se placent **uniquement dans un `.cpp`**.

Un point de vigilance : `NkTypeId` **n'est pas persistable** — il peut changer d'une compilation à
l'autre. On sérialise donc le **nom** du type, jamais son identifiant numérique.

> **En résumé.** Versionnement : `NkTypeOf<T>()` (ID sans RTTI, non persistable), `NkSchemaVersion`
> packée 64 bits (`==`/`!=`/`<`/`<=` seulement — pas de `>`), migrations enregistrées dans
> `NkSchemaRegistry`, appliquées en chaîne par `MigrateArchive` (garde 256 itérations, met à jour
> `__meta__`). Cas courant « ajouter un champ » via les macros `NK_ADD_FIELD_MIGRATION_*`, toutes dans
> un `.cpp`.

---

## La façade : `NkSerializer` et les formats

Une archive ne sert à rien tant qu'elle ne devient pas des octets sur un disque ou un câble. Le header
parapluie `NkSerializer.h` fournit la **couche haut niveau** qui relie `NkISerializable` ↔ `NkArchive`
↔ **format de fichier**. Les formats sont énumérés par `NkSerializationFormat` : JSON, XML, YAML,
binaire, natif (`NKS1`), et `NK_AUTO_DETECT`.

L'auto-détection est ce qui rend le module agréable : par **extension** (`.json`, `.xml`, `.yaml`/`.yml`,
`.bin`, `.nk`/`.nkasset`) ou par **nombres magiques** en tête de buffer (`"NKS1"` → natif, `"NKS\0"` →
binaire, `'{'` → JSON, `'<'` → XML). Vous pouvez donc, le plus souvent, ne rien préciser du tout.

```cpp
Enemy goblin; /* ... */
SaveToFile(goblin, "goblin.json");          // format déduit de l'extension → JSON
SaveToFile(goblin, "goblin.nk");            // → natif binaire

Enemy reloaded;
LoadFromFile("goblin.json", reloaded);      // déduit par extension puis magic bytes
```

Au plus haut niveau, les templates `Serialize<T>` / `Deserialize<T>` (vers/depuis une `NkString` pour
les formats texte), `SerializeBinary<T>` / `DeserializeBinary<T>` (vers/depuis un `NkVector<nk_uint8>`),
et `SaveToFile<T>` / `LoadFromFile<T>` opèrent directement sur vos objets `NkISerializable` (contrôlé
par `static_assert`). En dessous, les fonctions libres `SerializeArchiveText` / `...Binary` et leurs
symétriques travaillent sur une `NkArchive` brute — utiles quand vous construisez le document à la main.

Enfin, la factory polymorphe `CreateFromArchive(arc)` lit le champ `"__type__"` de l'archive,
ré-instancie le bon type via `NkSerializableRegistry`, appelle son `Deserialize`, et rend un
`NkUniquePtr<NkISerializable>` (RAII — pas de `delete` manuel). C'est le mécanisme qui permet de
recharger une scène hétérogène sans connaître à l'avance le type de chaque objet.

> **En résumé.** `NkSerializer.h` = façade haut niveau objet↔archive↔fichier. `NkSerializationFormat`
> (JSON/XML/YAML/binaire/natif/auto). Auto-détection par **extension** puis **magic bytes**.
> `SaveToFile`/`LoadFromFile`, `Serialize`/`Deserialize` (texte), `...Binary`. `CreateFromArchive`
> ré-instancie un type polymorphe via `"__type__"` et rend un `NkUniquePtr` (RAII).

---

## Aperçu de l'API

Tous les éléments publics, regroupés par fichier. Complexités et `noexcept` entre crochets quand c'est
utile.

### `NkArchive.h` — le modèle de document

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Énum | `NkArchiveValueType` (`NK_VALUE_NULL/BOOL/INT64/UINT64/FLOAT64/STRING`) | Discriminant scalaire. |
| Énum | `NkNodeKind` (`NK_NODE_SCALAR/OBJECT/ARRAY`) | Discriminant de nœud. |
| Valeur | `NkArchiveValue` : `type`, `raw` (union), `text` | Scalaire portable double représentation (`raw`+texte canonique). |
| Valeur · factories | `Null` `FromBool` `FromInt32/64` `FromUInt32/64` `FromFloat32/64` `FromString` `[noexcept]` | Constructions typées (`%.17g` pour float64). |
| Valeur · prédicats | `IsNull` `IsBool` `IsInt` `IsUInt` `IsFloat` `IsString` `IsNumeric` `[noexcept]` | Tests de type. |
| Valeur · opérateurs | `operator==` `operator!=` `[noexcept]` | Compare **type + texte** (pas `raw`). |
| Nœud | `NkArchiveNode` : `kind`, `value`, `object*`, `array` | Variant scalaire / objet (owning) / tableau ; copie **profonde**, non thread-safe. |
| Nœud · mutation | `SetObject` `MakeArray` `MakeScalar` `[noexcept]` | Change le type du nœud. |
| Nœud · prédicats | `IsScalar` `IsObject` `IsArray` `[noexcept]` | Tests de nature. |
| Entrée | `NkArchiveEntry` : `key`, `node` | Paire clé/nœud (ordre d'insertion préservé). |
| Archive · setters | `SetNull/Bool/Int32/Int64/UInt32/UInt64/Float32/Float64/String`, `SetValue` `[noexcept]` | Écrire un scalaire (clé plate). |
| Archive · getters | `GetValue/Bool/Int32/Int64/UInt32/UInt64/Float32/Float64/String` `[noexcept]` | Relire (coercition + parse texte ; float→int tronque). |
| Archive · objets | `SetObject` `GetObject` `[noexcept]` | Objet imbriqué (copie profonde). |
| Archive · tableaux | `SetArray` `GetArray` `SetNodeArray` `GetNodeArray` `[noexcept]` | Tableaux de valeurs / de nœuds. |
| Archive · nœuds | `SetNode` `GetNode` `FindNode` (const/mutable) `[noexcept]` | Nœud brut ; `FindNode` rend un pointeur **volatil**. |
| Archive · chemin | `SetPath` `GetPath` `[noexcept]` | API hiérarchique, séparateur `.`. |
| Archive · méta | `SetMeta` `GetMeta` `[noexcept]` | Sous la clé `"__meta__"`. |
| Archive · gestion | `Has` `Remove` `Clear` `Merge` `Size` `Empty` `Entries` (const/mutable) `[noexcept]` | Présence `O(n)`, suppression, fusion `O(m×n)`, accès brut. |

### `NkISerializable.h` — interface + registre par nom

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface | `NkISerializable` : `~`, `Serialize` `Deserialize` `GetTypeName` (virtuelles pures) | Se peindre / se relire / se nommer (chaîne statique). |
| Registre | `NkSerializableRegistry` (singleton par **nom**) | Ré-instancier un type depuis sa chaîne. |
| Registre · types | `FactoryFn`, `Entry` (`name[128]`, `factory`) | Type de la factory ; entrée. |
| Registre · API | `Global` `Register<T>` `RegisterFactory` `Unregister` `Create` `IsRegistered` `[noexcept]` | `Create` alloue par `new` brut (ownership au caller). |
| Macro | `NK_REGISTER_SERIALIZABLE(Type)` | Auto-enregistrement (dans un `.cpp` **uniquement**). |

### `NkSerializableRegistry.h` — registre par `NkTypeId` (ECS)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Registre | `NkComponentRegistry` (singleton par **`NkTypeId`**) | (Dé)sérialisation de composants ECS. |
| Types | `SerializeFn` `DeserializeFn` `Entry` (`typeId`, `name`, `serialize`, `deserialize`) | Fonctions et entrée. |
| API | `Global` `Register<T>` `Unregister<T>` `Find`/`FindByName`/`Find<T>` `Serialize`/`Deserialize` (id & `<T>`) `ForEach` `Count` `[noexcept]` | Synthèse par défaut : délégation `NkISerializable` ou dump hexa `__raw__`/`__rawSz__`. |
| Macros | `NK_REGISTER_COMPONENT(Type, Name)` · `NK_REGISTER_COMPONENT_CUSTOM(Type, Name, SerFn, DeserFn)` | Auto-enregistrement. |

### `NkSchemaVersioning.h` — versionnement & migrations

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identité | `NkTypeId` (= `nk_uint64`), `NkTypeOf<T>()` `[constexpr noexcept]` | ID de type sans RTTI ; **non persistable**. |
| Version | `NkSchemaVersion` : `major/minor/patch`, `Pack` `Unpack` `ToString`, `== != < <=` `[noexcept]` | Version sémantique packée 64 bits (pas de `>`/`>=`). |
| Migration | `NkSchemaMigration` : `MigrationFn`, `from`, `to`, `fn`, `Apply` `[noexcept]` | Une transformation version→version. |
| Adaptateur | `NkFieldMigrationAdapter<R>` : `Migrate` `InitSetter` `[noexcept]` | Interne : ajout de champ par défaut (idempotent). |
| Registre | `NkSchemaRegistry` : `Global` `SetCurrentVersion` `RegisterMigration` `MigrateArchive` `AddFieldMigration` `[noexcept]` | Chaîne de migrations (garde 256), met à jour `__meta__`. |
| Macros | `NK_SCHEMA_CURRENT_VERSION` · `NK_REGISTER_MIGRATION` · `NK_ADD_FIELD_MIGRATION_BOOL/INT32/FLOAT64/STRING` | Auto-enregistrement (dans un `.cpp`). |

### `NkSerializer.h` — façade & formats

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Énum | `NkSerializationFormat` (`NK_JSON/XML/YAML/BINARY/NATIVE/AUTO_DETECT`) | Format cible. |
| Détection (`detail`) | `DetectFormatFromExtension` `DetectFormatFromMagic` `IsBinaryFormat` `[inline noexcept]` | Auto-détection extension / magic bytes. |
| Archive · texte | `SerializeArchiveText` `DeserializeArchiveText` `[inline noexcept]` | `NkArchive` ↔ `NkString` (JSON/XML/YAML). |
| Archive · binaire | `SerializeArchiveBinary` `DeserializeArchiveBinary` `[inline noexcept]` | `NkArchive` ↔ `NkVector<nk_uint8>` (binaire/natif). |
| Objet · texte | `Serialize<T>` `Deserialize<T>` `[noexcept]` | Objet `NkISerializable` ↔ texte. |
| Objet · binaire | `SerializeBinary<T>` `DeserializeBinary<T>` `[noexcept]` | Objet ↔ octets. |
| Objet · fichier | `SaveToFile<T>` `LoadFromFile<T>` `[noexcept]` | Via `NkFile` (auto-détection extension/magic). |
| Factory | `CreateFromArchive` `[inline noexcept]` | Ré-instancie via `"__type__"`, rend un `NkUniquePtr`. |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages dans les domaines du moteur.
Les éléments triviaux sont décrits brièvement ; les pièces centrales (archive, getters tolérants,
migrations) sont traitées à fond.

### `NkArchiveValue` — la valeur scalaire portable

C'est l'atome de toute archive. Sa **double représentation** (union binaire `raw` + texte canonique
`text`) est ce qui lui permet d'être à la fois efficace en binaire et exacte en texte. Les factories
`From*` sont les seules constructions recommandées : `FromInt32`/`FromUInt32`/`FromFloat32` *promeuvent*
vers la représentation 64 bits interne, et `FromFloat64` formate en `%.17g` pour un round-trip sans
perte. Les prédicats (`IsNull`/`IsBool`/`IsInt`/`IsUInt`/`IsFloat`/`IsString`/`IsNumeric`) sont
`noexcept` et inline. Le piège à retenir : `operator==` compare **type + texte**, pas `raw` — deux
valeurs construites différemment mais au même texte canonique sont *égales*.

- **IO / réseau** : la couche élémentaire de tout paquet ou fichier sérialisé ; le texte canonique
  garantit qu'un flottant traversant JSON puis relu redonne le même bit-pattern.
- **Outils / éditeur** : un inspecteur peut afficher `text` directement, sans connaître le type
  concret du champ.
- **Gameplay** : valeurs de configuration, paramètres de spawn, seuils — tout ce qui est scalaire.

### `NkArchiveNode` — le variant de nœud

Un nœud est soit un scalaire (`NkArchiveValue`), soit un objet imbriqué (un `NkArchive*` **owning**,
détruit par le destructeur), soit un tableau (`NkVector<NkArchiveNode>`). C'est un **variant manuel** :
copie et déplacement font une **copie profonde** (l'objet imbriqué est dupliqué via `new NkArchive`).
`SetObject` y range une copie profonde d'une archive, `MakeArray` et `MakeScalar` rebasculent le type
en libérant l'objet courant. Les prédicats `IsScalar`/`IsObject`/`IsArray` qualifient la nature —
notez que `IsObject` exige `kind == OBJECT` **et** `object != nullptr`. **Non thread-safe.**

- **Rendu / scène** : un graphe de scène hiérarchique se représente naturellement en nœuds objets
  imbriqués (transform → enfants → maillage).
- **ECS** : une entité = un objet, ses composants = des sous-objets ou un tableau de nœuds.
- **UI / 2D** : un arbre de widgets se sérialise en nœuds objets/tableaux récursifs.

### `NkArchiveEntry` — la paire clé/nœud

Trivial : une `NkString key` et un `NkArchiveNode node`, stockés dans un `NkVector<NkArchiveEntry>` qui
**préserve l'ordre d'insertion**. C'est ce qui rend un fichier sérialisé lisible et stable (les champs
sortent dans l'ordre où on les a écrits). On y accède en masse via `NkArchive::Entries()`.

### `NkArchive` — les setters scalaires

`SetNull`/`SetBool`/`SetInt32`/`SetInt64`/`SetUInt32`/`SetUInt64`/`SetFloat32`/`SetFloat64`/`SetString`
écrivent une valeur sous une clé (validée par `IsValidKey`), et `SetValue` est la méthode centrale sur
laquelle reposent les autres. Tous renvoient un `nk_bool` (succès) et sont `noexcept`. Écrire une clé
déjà présente la remplace.

- **Gameplay / IA** : peindre l'état d'un agent (pv, position, état de la FSM) dans une archive de
  sauvegarde.
- **Audio** : sérialiser un mix (volume de bus, état des effets) ou un descripteur de son.
- **Threading** : construire l'archive sur le thread propriétaire — l'absence de garantie thread-safe
  impose de ne pas la partager pendant l'écriture.

### `NkArchive` — les getters tolérants

C'est le détail qui rend l'API robuste. `GetBool`/`GetInt32`/`GetInt64`/`GetUInt32`/`GetUInt64`/
`GetFloat32`/`GetFloat64`/`GetString`/`GetValue` écrivent dans un paramètre `out` et renvoient un
`nk_bool` indiquant si la lecture a réussi. Ils **coercent** entre types numériques et savent **parser
depuis le texte** : un entier stocké relu en flottant, un texte `"3.14"` relu en `float64`. La
troncature s'applique sur `float→int`. `GetInt32` délègue à `GetInt64` puis vérifie les bornes ;
`GetFloat32` délègue à `GetFloat64`. `GetString` renvoie **toujours** le texte canonique, quel que soit
le type stocké.

- **IO / réseau** : tolérer une donnée écrite par une version différente (un champ promu d'`int` à
  `float`) sans casser la lecture.
- **Outils / éditeur** : lire n'importe quel champ en chaîne (`GetString`) pour l'afficher, sans
  connaître son type.
- **Physique / animation** : relire des paramètres numériques (masse, durée, courbes) avec coercition
  automatique selon ce que le code attend aujourd'hui.

### `NkArchive` — objets, tableaux, nœuds bruts

`SetObject`/`GetObject` imbriquent une archive (copie profonde dans les deux sens). **Piège Windows** :
`GetObject` fait un `#undef GetObject` local pour neutraliser le `GetObjectA` de `windows.h`.
`SetArray`/`GetArray` manipulent des tableaux de valeurs scalaires (`GetArray` **filtre** les nœuds non
scalaires), tandis que `SetNodeArray`/`GetNodeArray` portent des tableaux de nœuds arbitraires (objets
et tableaux compris). Au plus bas, `SetNode`/`GetNode` posent ou lisent un nœud brut, et `FindNode`
(const et mutable) renvoie un **pointeur direct** dans la table.

> **Avertissement.** Le pointeur de `FindNode` (et de `GetPath`) est **non-owning** et **invalidé par
> toute mutation** de l'archive (insertion, suppression, `Merge`…). À consommer **immédiatement**,
> jamais à conserver, jamais à `delete`.

- **Rendu / scène** : un tableau de nœuds = la liste hétérogène des enfants d'un nœud de scène.
- **ECS** : `GetNodeArray` relit la liste des composants d'une entité, chacun étant un sous-objet.
- **GPU / assets** : décrire un matériau (objet) avec un tableau de passes (tableau de nœuds).

### `NkArchive` — chemins hiérarchiques

`SetPath("a.b.c", node)` **crée au passage** les objets intermédiaires manquants (coût
`O(depth × entries_per_level)`), et `GetPath("a.b.c")` renvoie un pointeur (`nullptr` si un maillon
manque). C'est l'API confortable pour adresser un champ profond sans descendre manuellement objet par
objet.

- **Outils / éditeur** : un inspecteur qui édite `"transform.position.x"` sans naviguer la hiérarchie à
  la main.
- **Gameplay** : lire un réglage profond d'un fichier de configuration (`"audio.master.volume"`).

### `NkArchive` — méta-données et gestion

`SetMeta`/`GetMeta` rangent des paires sous la clé réservée `"__meta__"` — c'est là que le
versionnement écrit `schema_version`. Côté gestion : `Has` (présence, `O(n)`), `Remove`, `Clear`,
`Merge(other, overwrite)` (fusion `O(m×n)`, écrasement optionnel), `Size`/`Empty`, et `Entries()`
(const et mutable) pour itérer ou réordonner manuellement les entrées.

- **Outils** : `Merge` applique un *patch* ou un *preset* par-dessus une archive de base.
- **IO** : `Has` teste l'existence d'un champ optionnel avant de le lire ; les méta portent
  l'estampille de version et l'origine.

### `NkISerializable` — l'interface de base

Trois virtuelles pures : `Serialize`/`Deserialize` (mêmes champs, **même ordre**) et `GetTypeName`
(chaîne **statique** à vie globale). Le destructeur est virtuel. C'est le contrat minimal pour qu'un
type entre dans toute la machinerie haut niveau (`Serialize<T>`, `CreateFromArchive`).

- **ECS** : chaque composant non trivial implémente l'interface pour un contrôle fin de son écriture.
- **Gameplay / IA** : entités, états de FSM, objets de mission se rendent persistables par ce biais.
- **Audio** : un graphe d'effets ou une banque de sons décrit sa propre sérialisation.

### `NkSerializableRegistry` — ré-instancier par nom

Singleton (`Global()`). `Register<T>("Nom")` requiert un constructeur par défaut ;
`RegisterFactory("Nom", fn)` couvre les types sans ctor par défaut. `Create("Nom")` rend un
`NkISerializable*` **alloué par `new` brut** — c'est à l'appelant de `delete` (sauf via
`CreateFromArchive`, qui RAII). `IsRegistered`/`Unregister` complètent. Les `Entry` ont un `name[128]`
fixe (127 caractères + nul). **Register/Unregister ne sont pas thread-safe** : enregistrez au démarrage,
mono-thread ; la lecture concurrente après init est sûre. La macro `NK_REGISTER_SERIALIZABLE(Type)`
automatise, **dans un `.cpp` uniquement** (sinon violation d'ODR).

- **Outils / éditeur** : recharger une scène contenant des types hétérogènes, ré-instanciés par leur
  nom écrit dans le fichier.
- **IO / réseau** : recréer côté récepteur un objet décrit par son nom de type.

> **Note ownership.** Les deux registres (`NkSerializableRegistry`, `NkComponentRegistry`) allouent leurs
> factories par `new`/`delete` C++ — une **exception locale** à la règle NKMemory du projet. Pour les
> objets que vous récupérez de `Create`, faites le `delete` (ou passez par `CreateFromArchive`).

### `NkComponentRegistry` — par `NkTypeId`, pour l'ECS

Distinct du précédent : indexé par **`NkTypeId`**, header-only, tout statique inline. `Register<T>("Nom")`
synthétise des (dé)sérialiseurs par défaut si on n'en fournit pas — délégation à `NkISerializable` si
disponible, sinon **dump hexa brut** des `sizeof(T)` octets sous `__raw__`/`__rawSz__`. Ce dump n'est
**pas portable** (dépend du layout et de la plateforme) : utile en prototypage, à proscrire pour des
sauvegardes durables. On retrouve un type par `Find(id)`/`FindByName`/`Find<T>`, on (dé)sérialise par
`Serialize(id, …)` ou `Serialize<T>(…)`, on itère par `ForEach`, on compte par `Count`. Macros
`NK_REGISTER_COMPONENT` et `NK_REGISTER_COMPONENT_CUSTOM`.

- **ECS** : sérialiser en masse tous les composants d'un monde, type par type, sans dispatch manuel.
- **Outils** : `ForEach` alimente un éditeur de composants générique.

### `NkTypeId` et `NkTypeOf<T>()`

`NkTypeId` est un `nk_uint64` ; `NkTypeOf<T>()` génère un identifiant unique par type via l'adresse d'un
`static const char` local (technique EnTT, **sans RTTI**), stable au sein d'une compilation. Comparaison
`O(1)`. **À ne jamais persister** : il peut changer d'une compilation à l'autre — sérialisez le *nom*
du type.

- **ECS / threading** : clé rapide pour indexer des registres ou router un message à la volée.

### `NkSchemaVersion` — la version sémantique

`major/minor/patch` (16 bits chacun) packés en 64 bits par `Pack()` (`(major<<32)|(minor<<16)|patch`),
dépaqués par `Unpack`. `ToString()` rend `"maj.min.patch"`. Les comparateurs déclarés sont **`==`,
`!=`, `<`, `<=` uniquement** (ils comparent `Pack()`) : il n'y a **pas** de `>` ni `>=` — pensez à
inverser les opérandes si besoin.

- **IO** : étiqueter chaque archive avec la version du schéma qui l'a produite.

### `NkSchemaMigration` et `NkFieldMigrationAdapter`

`NkSchemaMigration` est un triplet `{from, to, fn}` ; `Apply` exécute `fn` (ou renvoie `true` si `fn`
est nul). `NkFieldMigrationAdapter<R>` est l'**adaptateur interne** du cas « ajouter un champ avec une
valeur par défaut » : il capture le défaut dans un `NkFunction` (STL-free), résout le bon setter
d'archive selon `R` via `if constexpr` (bool, int32/uint32/int64/uint64, float32/float64, `NkString`),
et son `Migrate` est **idempotent** (no-op si le champ existe déjà ; `false` si le type n'est pas
supporté). Vous n'instanciez pas l'adaptateur à la main — les macros `NK_ADD_FIELD_MIGRATION_*` le font.

### `NkSchemaRegistry` — orchestrer les migrations

Singleton header-only. `SetCurrentVersion(typeId, version)` déclare la version courante d'un type ;
`RegisterMigration(typeId, from, to, fn)` ajoute un maillon ; `AddFieldMigration<DefaultFn>(...)` ajoute
le maillon « champ par défaut » en déduisant le type de retour. Le cœur est
`MigrateArchive(typeId, archive, stored, err)` : il enchaîne les migrations de `stored` jusqu'à la
version courante, met à jour `"__meta__".schema_version` en cas de succès, et renvoie des erreurs
parlantes (`"Type not registered for versioning"`, `"Migration X → Y failed"`,
`"No migration path from X to Y"`). Une **garde de 256 itérations** empêche toute boucle infinie ; le
coût est `O(256 × nb_migrations)`. **L'enregistrement n'est pas thread-safe** (au démarrage).

- **IO / outils / gameplay** : charger une vieille sauvegarde et l'amener automatiquement au schéma
  courant avant désérialisation — l'utilisateur ne voit jamais la migration.

### `NkSerializationFormat` et la détection (`detail`)

L'énum couvre JSON, XML, YAML, binaire, natif, et `NK_AUTO_DETECT`. `DetectFormatFromExtension` mappe
`.json`/`.xml`/`.yaml`/`.yml`/`.bin`/`.nk`/`.nkasset` (sensible à la casse, extension simple seulement) ;
`DetectFormatFromMagic` lit l'en-tête (`"NKS1"`→natif, `"NKS\0"`→binaire, `'{'`→JSON, `'<'`→XML, sinon
binaire) ; `IsBinaryFormat` vrai pour binaire ou natif.

- **IO / outils** : ouvrir un fichier inconnu sans imposer son format à l'utilisateur.

### Les fonctions d'archive (texte / binaire)

`SerializeArchiveText`/`DeserializeArchiveText` convertissent une `NkArchive` ↔ `NkString` pour les
formats texte (le paramètre `pretty` est ignoré pour YAML ; renvoie `false` sur un format binaire).
`SerializeArchiveBinary`/`DeserializeArchiveBinary` font le lien avec un `NkVector<nk_uint8>` (binaire ou
natif ; `AUTO_DETECT` traité comme natif à l'écriture, via magic bytes à la lecture). Toutes acceptent un
`NkString* outError` optionnel.

- **GPU / assets** : exporter une archive d'asset en natif binaire compact pour le runtime, en JSON
  lisible pour l'outillage — depuis le **même** document.

### Les templates objet (texte / binaire / fichier)

`Serialize<T>`/`Deserialize<T>` (texte), `SerializeBinary<T>`/`DeserializeBinary<T>` (octets), et
`SaveToFile<T>`/`LoadFromFile<T>` opèrent directement sur un objet `NkISerializable` (vérifié par
`static_assert`). Les variantes fichier passent par `NkFile` (`NK_WRITE_BINARY` / `NK_READ`) et
auto-détectent le format (extension à l'écriture ; extension puis magic bytes à la lecture, fallback
natif). **Ces fonctions ne sont pas thread-safe vis-à-vis du système de fichiers** — ne lisez/écrivez
pas le même chemin depuis plusieurs threads.

- **Gameplay** : `SaveToFile(state, "save.nk")` / `LoadFromFile("save.nk", state)` — sauvegarde de
  partie en une ligne.
- **Outils** : exporter un même objet en `.json` (revue, diff git) et `.nk` (runtime).

> **Incohérence connue.** `Serialize`/`SerializeBinary`/`SaveToFile` testent la parenté via
> `std::is_base_of`, alors que `LoadFromFile` utilise `traits::NkIsBaseOf` (malgré le commentaire
> d'en-tête prétendant l'usage de `traits::NkIsBaseOf` partout). Sans effet fonctionnel, mais à savoir.

### `CreateFromArchive` — la factory polymorphe

`CreateFromArchive(arc)` lit le champ string `"__type__"`, instancie le type via
`NkSerializableRegistry::Global().Create(...)`, appelle `Deserialize`, et rend un
`NkUniquePtr<NkISerializable>` (RAII — pas de `delete` manuel). Renvoie `nullptr` si `"__type__"` est
absent, si le type est inconnu, ou si `Deserialize` échoue. C'est la clé de voûte du chargement de
collections hétérogènes.

- **Outils / scène** : recharger une scène où chaque objet déclare son type dans `"__type__"`.
- **IO / réseau** : reconstruire côté récepteur l'objet exact décrit par le message.

---

### Exemple récapitulatif

```cpp
#include "NKSerialization/NkSerializer.h"
using namespace nkentseu;

// 1) Un type sérialisable + son enregistrement (dans le .cpp).
struct Enemy : NkISerializable {
    NkInt32 hp = 0; NkString name; nk_float32 speed = 0.f;
    nk_bool Serialize(NkArchive& a) const override {
        a.SetString("__type__", GetTypeName());   // requis pour CreateFromArchive
        a.SetInt32("hp", hp);
        a.SetString("name", name.View());
        a.SetFloat32("speed", speed);
        return true;
    }
    nk_bool Deserialize(const NkArchive& a) override {
        a.GetInt32("hp", hp);
        a.GetString("name", name);
        a.GetFloat32("speed", speed);              // coercition automatique
        return true;
    }
    const char* GetTypeName() const noexcept override { return "Enemy"; }
};
NK_REGISTER_SERIALIZABLE(Enemy);                   // .cpp uniquement

// 2) Versionnement : la 1.0.0 n'avait pas "speed", on l'ajoute par migration.
NK_SCHEMA_CURRENT_VERSION(Enemy, 2, 0, 0);
NK_ADD_FIELD_MIGRATION_FLOAT64(Enemy, 1,0,0,  2,0,0,  "speed", 1.0);

// 3) Sauvegarde / chargement avec auto-détection du format.
Enemy goblin{ 30, "Goblin", 2.5f };
SaveToFile(goblin, "goblin.json");                 // JSON (déduit de l'extension)
SaveToFile(goblin, "goblin.nk");                   // natif binaire

Enemy reloaded;
LoadFromFile("goblin.nk", reloaded);               // déduit par extension puis magic bytes

// 4) Chargement polymorphe via le champ "__type__".
NkArchive arc;
DeserializeArchiveText(jsonText, NkSerializationFormat::NK_JSON, arc);
NkUniquePtr<NkISerializable> obj = CreateFromArchive(arc);   // RAII, pas de delete
```

---

[← Index NKSerialization](README.md) · [Récap NKSerialization](../NKSerialization.md) · [Couche System](../README.md)
