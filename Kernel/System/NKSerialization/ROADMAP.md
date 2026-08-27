# NKSerialization — Roadmap

État actuel (mai 2026) : **Module le plus mature du dossier System après
NKLogger/NKThreading**. Architecture mature autour d'une `NkArchive` clé/valeur
hiérarchique servant d'intermédiaire universel. **5 formats round-trip
fonctionnels** (JSON, XML, YAML, Binary générique, NkNative binaire avec
header+CRC32). **Système d'assets complet** (NkAssetId GUID 128-bit,
NkAssetMetadata, NkAssetRegistry, format `.nkasset` = Header+Metadata+Payload).
**15 tests smoke en suite standalone**, tous formats validés. Outil CLI
`nkecs-convert`. Reste à câbler en profondeur les formats projet
`.nkproj`/`.nkscene`/`.nkcase`/`.nkb` (cf. ARCHITECTURE.md §8), la réflexion
runtime via NKReflection, et le versioning de schéma.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| NkArchive (scalaires, objets, arrays, paths "a.b.c", meta __meta__) | Livré | — | — |
| NkArchiveValue + NkArchiveNode + NkArchiveValueType (NULL/BOOL/INT64/UINT64/FLOAT64/STRING) | Livré | — | — |
| NkISerializable + NkSerializableRegistry (factory polymorphique) | Livré | — | — |
| NkSerializer (API haut niveau Serialize/Deserialize, SaveToFile/LoadFromFile) | Livré | — | — |
| Détection format auto (extension + magic bytes) | Livré | — | — |
| JSON reader/writer + NkJSONValue | Livré | — | — |
| XML reader/writer | Livré | — | — |
| YAML reader/writer | Livré | — | — |
| Binary generic reader/writer | Livré | — | — |
| NkNative format (header + archive + payload + CRC32) | Livré | — | — |
| NkReflect statique (macros NK_REFLECT_BEGIN/END, sérialisation auto struct C++) | Livré | — | — |
| NkAssetId GUID 128-bit + NkAssetPath logique "/Game/..." | Livré | — | — |
| NkAssetMetadata (tags, properties, dependencies, version) | Livré | — | — |
| NkAssetRegistry (scan + lookup par type/path) | Livré | — | — |
| NkAssetIO Write/Read fichier `.nkasset` | Livré | — | — |
| NkSchemaVersioning (compat ascendante/descendante) | Partiel | M | Haute |
| NkAssetImporter (pipeline source → .nkasset) | Partiel | L | Haute |
| Suite de tests 15 cas (Archive, formats, AssetIO, Registry, Merge) | Livré | — | — |
| CLI tool `nkecs-convert` | Livré | — | — |
| Câblage formats projet `.nkproj` / `.nkscene` / `.nkcase` / `.nkb` | TODO | L | Haute |
| Intégration NKReflection runtime (Phase NkSerializeReflect<T>) | TODO | L | Haute |
| Hot-reload via NkFileWatcher (assets) | TODO | M | Moyenne |
| Compression LZ4/Zstd dans NkNative payload | TODO | M | Moyenne |
| Streaming partiel (load partiel d'archive énorme) | TODO | L | Basse |
| Benchmarks réels (NkSerializationBenchmark commenté actuellement) | TODO | S | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Cœur — NkArchive
- [NkArchive](src/NKSerialization/NkArchive.h) :
  - Hiérarchie : `NkArchiveValue` (scalaire portable) → `NkArchiveNode`
    (scalaire | objet | array) → `NkArchive` (table associative ordonnée).
  - Scalars : NULL, BOOL, INT64, UINT64, FLOAT64, STRING (UTF-8).
  - API plate : `SetString/Int64/UInt64/Bool/Float64`,
    `GetString/Int64/...`, `Has`, `Remove`.
  - API hiérarchique : `SetObject(name, child)`, `GetObject`, `SetArray`,
    `GetArray<NkArchiveValue>`, `SetPath("a.b.c", node)`, `GetPath(...)`.
  - Métadonnées : `SetMeta("author", ...)`, `GetMeta`.
  - `Merge(other, overwrite)` pour fusion.
  - **Trivia — mise à jour du 2026-08-21** : `NkArchiveTrivia`, bloc
    **facultatif** attaché aux nœuds et à l'archive. Voir la section dédiée
    ci-dessous.

### Interface et registry
- [NkISerializable](src/NKSerialization/NkISerializable.h) : interface
  STL-free avec `Serialize(NkArchive&)` / `Deserialize(const NkArchive&)`.
- [NkSerializableRegistry](src/NKSerialization/NkSerializableRegistry.h) :
  factory polymorphique. `Register<T>("Player")`, `Create(typeName)` →
  `NkUniquePtr<NkISerializable>`. Thread-safe en lecture après init.
- `CreateFromArchive(archive)` : lit `__type__` et délègue au registry.

### API haut niveau
- [NkSerializer](src/NKSerialization/NkSerializer.h) :
  - Énum [NkSerializationFormat](src/NKSerialization/NkSerializer.h) :
    NK_JSON / NK_XML / NK_YAML / NK_BINARY / NK_NATIVE / NK_AUTO_DETECT.
  - `Serialize<T>(obj, fmt, NkString&)` texte +
    `SerializeBinary<T>(...)` → `NkVector<uint8>`.
  - `Deserialize<T>(text, fmt, obj)` /
    `DeserializeBinary<T>(data, size, fmt, obj)`.
  - `SaveToFile<T>(obj, path, fmt)` / `LoadFromFile<T>(path, obj, fmt)`.
  - Détection auto via extension (`.json`/`.xml`/`.yaml`/`.yml`/`.bin`/`.nk`/
    `.nkasset`) puis magic bytes (`NKS1` Native, `NKS\0` Binary, `{` JSON,
    `<` XML).
  - `static_assert` que `T : NkISerializable`.
  - API bas niveau `SerializeArchiveText/Binary` /
    `DeserializeArchiveText/Binary` opérant directement sur `NkArchive`.

### Formats
- **JSON** : [NkJSONReader / NkJSONWriter / NkJSONValue](src/NKSerialization/JSON/) :
  parsing + écriture pretty/compact, support nested objects + arrays.
- **XML** : [NkXMLReader / NkXMLWriter](src/NKSerialization/XML/) avec
  écriture pretty.
- **YAML** : [NkYAMLReader / NkYAMLWriter](src/NKSerialization/YAML/),
  indentation minimale.
- **Binary** générique : [NkBinaryReader / NkBinaryWriter](src/NKSerialization/Binary/) :
  format compact non typé strict.
- **NkNative** : [NkNativeFormat](src/NKSerialization/Native/NkNativeFormat.h) :
  magic `NKS1`, header versionné, archive sérialisée, CRC32 final.
- **NkReflect** : [NkReflect.h](src/NKSerialization/Native/NkReflect.h) :
  réflexion **statique** compile-time via macros `NK_REFLECT_BEGIN/END`,
  SFINAE-based, type-safe sans RTTI runtime. Couvre scalaires + NkString +
  types imbriqués.

### Système d'assets
- [NkAssetMetadata](src/NKSerialization/Asset/NkAssetMetadata.h) :
  - `NkAssetId` : GUID 128-bit (`{hex16-hex16}`), `Generate()` / `ToString()`
    / `FromString()`.
  - `NkAssetPath` : chemin logique style Unreal `/Game/Meshes/Cube`.
  - `NkAssetMetadata` : id, assetPath, type (StaticMesh / Texture2D /
    Material / ...), typeName, version, tags, properties (NkArchive),
    dependencies (vector de `{AssetId, AssetPath}`).
  - `NkAssetRegistry` : `Register(record)`, `FindByPath`, `GetByType`,
    `SaveRegistry(.nkreg)`, `LoadRegistry(.nkreg)`, `Count()`.
  - `NkAssetIO` : `Write(path, meta, payload, size)`,
    `ReadMetadata(path, meta)`, `ReadPayload(path, vector)`.
  - Format `.nkasset` : `[FileHeader:32][MetadataSize:4][Metadata:NkNative]
    [PayloadSize:8][Payload:bytes]`.
- [NkAssetImporter](src/NKSerialization/Asset/NkAssetImporter.h) : pipeline
  source → .nkasset (header-only, à enrichir).

### ⚠️ Les bancs de `tests/` NE S'EXECUTENT PAS — mesuré le 2026-08-21

`jenga test --project NKSerialization --config Debug` répond
**« Unit-test execution is disabled by workspace policy (`disableunittestexecution`) »**.
Les `testfiles(["tests/**.cpp"])` du `.jenga` (l. 77-80) ne tournent jamais, et
`find Build -ipath "*NKSerialization*" -name "*.exe"` ne renvoie rien.

⚠️ **CORRECTION du 2026-08-22 — j'avais écrit « ils compilent sans jamais
tourner ». C'est faux : ils NE COMPILENT PAS NON PLUS.** Mesuré avec
`jenga build --target NKSerialization_Tests --config Debug --tests` (la cible
existe, `dutc` l'exclut simplement du build par défaut) : **8 erreurs, 2 fichiers
en échec**, dont trois causes distinctes :

| cause | où | nature |
|---|---|---|
| `std::move` × 5 | `src/NKSerialization/Asset/NkAssetMetadata.h` | **violation zero-STL dans du code LIVRÉ** — et le fait qu'elle ne casse pas la lib prouve qu'aucun `.cpp` n'inclut ce header : il n'est jamais instancié, donc jamais compilé |
| `NkVector<nk_uint8> payload(2048u)` ambigu | `tests/test_smoke.cpp:399` | test périmé vis-à-vis de l'API actuelle de `NkVector` |
| `NKMath/NkVec.h` introuvable | via `NKReflection/NkMathReflect.h` | dépendance manquante dans la cible de tests |

**La leçon est plus dure que celle d'hier.** Je disais « un module dont la preuve
vit sous `tests/` n'est ni prouvé ni réfuté ». Il faut ajouter : **un header-only
que personne n'instancie n'est même pas compilé** — `NkAssetMetadata.h` (891
lignes) viole le zero-STL depuis on ne sait quand, et rien dans le dépôt ne
pouvait le dire. Chercher `std::` par `grep` trouve en une seconde ce que le
compilateur ne dira jamais.

**Non corrigé ici, délibérément** : hors mandat du chantier NkUIDesign, et
surtout corriger `NkAssetMetadata.h` sans banc qui l'instancie ne prouverait
rien — ce serait refaire exactement l'erreur que cette section décrit.

**`jenga test --force` existe depuis le 2026-08-22** et lève bien `dutc`/`dute`,
mais répond `No test projects found` : `dutc` empêche la *création* de la cible,
que `--force` ne recrée pas. Il faut passer par
`jenga build --target <Projet>_Tests --tests`. Les 15 tests de `test_smoke.cpp` et les trois bancs réflexion listés
ci-dessous **n'ont donc jamais produit un seul résultat**.

C'est la convention du dépôt : un banc doit être une **application console**
(voir `Applications/NKGuiDrawTest/NKGuiDrawTest.jenga` l. 10-13, qui documente
ce choix). D'où :

**`Sandbox/System/NKSerialization` — banc EXÉCUTABLE, 47/47 le 2026-08-21.**
`jenga build --target SandboxNKSerialization --config Debug` puis
`./Build/Bin/Debug-Windows/SandboxNKSerialization/SandboxNKSerialization.exe`
(code de sortie 0 = tout passe). Couvre C1 conteneur de scalaires, C2 conteneur
d'objets, C3 conteneurs **imbriqués** sur trois niveaux (la forme de
`NkUIDocument`), C4 provenance jamais omise même au défaut.

**`Sandbox/System/NKArchive` — second banc EXÉCUTABLE, 105/105 le 2026-08-21.**
`jenga build --target SandboxNKArchive --config Debug` puis
`./Build/Bin/Debug-Windows/SandboxNKArchive/SandboxNKArchive.exe`. Couvre la
mise à jour trivia / ordre / forme littérale de `NkArchive` — détail dans la
section dédiée plus bas.

**Ce que ça a coûté de ne pas l'avoir** : l'en-tête de `NkReflectSerializer.h`
annonçait la Phase 3 « repoussée, non gérée » alors que le `.cpp` l'implémentait
déjà. Aucun test ne tournant, rien ne contredisait l'en-tête, et cette
affirmation fausse a servi de base à une décision d'architecture du chantier
NkUIDesign. **Un en-tête est une promesse ; seul un banc qui tourne est une
preuve.**

### Phase 3 du pont réflexion — FAITE (et non « repoussée »)

Vérifié dans le corps de `Reflection/NkReflectSerializer.cpp`, pas dans son
en-tête, et **mesuré** par le banc ci-dessus :

| capacité | implémentation | état |
|---|---|---|
| conteneur de scalaires / strings `NkVector<T>` | `WriteContainerProperty` / `ReadContainerProperty` (l. 386 / 440) | ✅ |
| conteneur d'**objets réfléchis** (object-array récursif) | `WriteObjectContainerProperty` / `ReadObjectContainerProperty` (l. 358 / 416) | ✅ |
| conteneurs **imbriqués** (objets portant eux-mêmes des conteneurs) | par récursion de `SerializeReflected` | ✅ |
| **pointeurs** | — | ❌ **reste à faire** |

Support des deux côtés : `NKReflection/NkContainerTrait.h`
(`NkContainerDescriptor`, thunks templates spécialisés pour `NkVector<T>`) et
`NkArchive` (`SetArray`/`GetArray`, `SetObjectArray`/`GetObjectArray`,
`SetNodeArray`/`GetNodeArray`).

**Dette nommée — les pointeurs.** Seul morceau de Phase 3 non couvert.

⚠️ **CORRECTION du 2026-08-22 — j'avais écrit que cette dette était « sans objet
pour un document d'interface, `NkUIDocument` étant plat, zéro pointeur ». C'est
faux.** Vrai de `NkUINode` **directement**, faux **transitivement** : `NkUINode`
porte deux `NkSizeDecl` et un `NkLayoutDecl` (types du kit,
`NKEditorKit/Components/NkComponentLayout.h`), qui portent à eux trois **quatre
`const char *`** — `valueMetric`, `spacingMetric`, `padMetric`,
`gridCellMetric`. Ce ne sont pas des pointeurs d'objets : ce sont des **noms de
métrique**, déclarés ainsi parce que ces types sont des types de *compilation*
dont les chaînes sont des littéraux — le type ne possède rien.

**Et le comportement est pire que « non géré » — mesuré par C5 du banc :**

```
SerializeObject   -> true      <-- il dit avoir REUSSI
cle 'valueMetric' -> ABSENTE   <-- le champ est silencieusement OMIS
DeserializeObject -> true      <-- il dit avoir REUSSI
valueMetric       -> <<>>      <-- la metrique a disparu
```

Aucun signal, aucun code d'erreur. Une taille qui désigne `largeur_palette` se
relit **sans métrique** et se résout au nombre : le document survit à
l'aller-retour en ayant perdu sa raison d'être. Fait de typage sous-jacent :
la réflexion classe `const char *` en `NK_POINTER`, jamais `NK_STRING`
(`NkType.h` l. 500).

### ⚠️ Un type de VALEUR ne doit pas devenir POLYMORPHE pour être sérialisable

Contrainte de conception établie le 2026-08-22, et elle resservira bien au-delà
de ce module.

`NKENTSEU_REFLECT_CLASS` est **intrusive** : elle injecte dans la classe un
`using SelfType`, une méthode statique, **et une méthode virtuelle**
`GetClass()`. Appliquée à un type de valeur simple comme `NkSizeDecl` (cinq
champs, agrégat, aucun héritage), elle lui donnerait une **vtable** : `sizeof`
change, la disposition mémoire change, `offsetof` devient douteux, le type cesse
d'être un agrégat. Pour un type de mise en page instancié partout dans le kit,
c'est un coût structurel imposé par un besoin de sérialisation — l'inverse du
sens de dépendance souhaitable.

**La voie non intrusive existe et doit être préférée** :
`NkReflectSerializer::ResolveClass<T>()` (`NkReflectSerializer.h` l. 143-151) a
un repli SFINAE vers `NkRegistry::Get().GetClass<T>()` quand `T::GetStaticClass()`
n'existe pas. On peut donc construire un `NkClass` **depuis l'extérieur** —
`NkClass(nom, sizeof, NkTypeOf<T>())`, des `NkProperty(nom, type, offset)`,
`AddProperty`, puis `RegisterClass` — **sans toucher au type réfléchi**.

⚠️ **Piège dans le helper prévu pour ça.** `NkClass::RegisterMemberProperty`
(`NkClass.h` l. 524) déclare son `NkProperty` en `static` **local à une fonction
template** : il n'y en a donc **qu'un par instanciation `<ClassType, ValueType>`**.
Enregistrer deux `float32` sur la même classe rend deux fois **le même**
`NkProperty`, et la seconde propriété écrase la première. C'est de la sémantique
C++ pure, pas un comportement à mesurer. **Ne pas utiliser ce helper** pour plus
d'une propriété d'un type donné : déclarer les `NkProperty` en statiques nommés.

### Le pool de chaînes du document — (b), appliquée le 2026-08-22

`Applications/NKUIDesign/src/NKUIDesign/NkDocStringPool.h`. Le propriétaire qui
manquait aux quatre `const char *` du kit. **Trois contrats, pas trois détails
d'implémentation :**

**1. Durée de vie.** Une chaîne du pool vit **exactement** aussi longtemps que le
document qui la porte. Un pointeur rendu par `Intern()` ne doit jamais survivre à
son document — ni dans un cache, ni dans une capture, ni dans un rapport.

**2. Identité — PAS de déduplication, et c'est un choix.** Deux nœuds employant
`largeur_palette` reçoivent **deux adresses différentes**. Dédupliquer ferait
marcher la comparaison d'adresse comme test d'égalité de nom — mais **seulement à
l'intérieur d'un document** : deux documents, une copie, un import, et elle
redevient fausse. Quelqu'un l'essaierait, ça marcherait dans son test, et ça
casserait en silence bien plus tard. Sans déduplication, l'égalité d'adresse est
*systématiquement* fausse : le premier qui l'essaie échoue **tout de suite**.
**On préfère l'échec visible et immédiat à l'échec silencieux et différé.**

Mesure qui rend le choix sûr : **0 comparaison d'adresse** sur ces champs dans
tout le dépôt (vérifié le 2026-08-22 — toutes les résolutions passent par
`StrEq`, via `NkComponentDecl::FindMetric` et `NkUIDocument::Metric` ; les seuls
tests sur le pointeur sont `name && *name`). **Comparer par contenu, jamais par
adresse.**

**3. Stabilité des adresses — le piège qui tue ce genre de pool.** Un
`NkVector<NkString>` **reloge** ses éléments en grandissant : tous les
`const char *` déjà distribués deviennent pendouillants, et le document se
corrompt **à la Nième métrique, pas à la première**. D'où `NkVector<NkString *>` :
le vecteur de pointeurs peut se reloger, les `NkString` désignées ne bougent
jamais.

**Preuve — `NKUIDesign --pool-controles`, 528/528, sortie 0.** P1 stabilité des
adresses sous croissance forcée, P2 le contrat de non-déduplication, P3 la
contre-épreuve (un document **sans** métrique reste valide et ne coûte rien), P4
l'aller-retour du nom (la source meurt, le champ survit), P5 possession et
libération.

⚠️ **Mutation 10 — le pool naïf, `NkVector<NkString>` qui reloge : 525/528, et
P4 RESTE VERT.** L'aller-retour du nom — la preuve qu'on croyait décisive —
n'interne qu'une seule chaîne, donc ne déclenche aucune réallocation et **ne peut
pas voir** le défaut. Seul P1, qui force la croissance, l'attrape. **La preuve la
plus proche du besoin n'est pas celle qui attrape le plus** — troisième fois que
ce motif sort sur ce chantier.

#### Le pool **câblé** sur `NkUIDocument` — le 2026-08-22 au soir

Le pool prouvé isolément ne servait encore à rien : aucun document ne passait par
lui. Il l'est maintenant, et **l'absence nommée de `Document.h` l. 908 est levée**
— `NkSizeDecl::valueMetric` s'écrit, se relit et **prime sur le nombre**.

⚠️ **Il ne l'est PAS de la façon que ce commentaire annonçait.** Il disait que le
jour venu le champ se traiterait « comme `spacingName` », c'est-à-dire par une
`NkString` posée à côté. Ça aurait fait **deux vérités pour un seul fait** : le
champ que le solveur du kit lit (`NkLayoutSolve.h` l. 131) et la chaîne que le
document garde, à tenir d'accord pour toujours. Le pool donne un propriétaire au
**champ lui-même** : le solveur lit la seule vérité qui existe.

**Mesure qui a décidé de la conception : `NkUIDocument` EST copiée par valeur**
— cinq fois rien que dans `Probe.h`. Un pool non copiable rendait donc la classe
non copiable. D'où une copie explicite qui **re-interne** (`CopyValuesFrom`), et
un déplacement qui, lui, n'a rien à re-interner — le pool est un
`NkVector<NkString *>`, déplacer le vecteur ne déplace pas les `NkString`.

**Le 5e jeton n'est écrit que s'il est non vide** : un document qui ne nomme
aucune taille se réécrit **octet pour octet** comme avant.

**Preuve — `NKUIDesign --pool-controles`, 573/573, sortie 0.** D1 l'aller-retour
d'un document réel (le texte lu meurt, puis le solveur rend 240 et non le `1.f`
écrit dans `value`), D2 la compatibilité ascendante, D3 la copie, D4 la greffe,
D5 la croissance conjointe. Baselines intactes : `SandboxNKArchive` 114/114,
`SandboxNKSerialization` 63/63 + 1 dette connue, `--roundtrip-controles` 36/36,
`--probe` 103/103.

⚠️ **Six mutations, et les DEUX qui ont survécu au premier tour visaient chacune
un contrôle que je venais d'écrire pour elles.**

- **Mutation E — écrire *toujours* le 5e jeton, même vide : 573/573, verte.** Le
  contrôle « octet pour octet » comparait deux textes produits par **le même
  écrivain** : quand l'écrivain change de format, les deux changent ensemble et
  la comparaison reste vraie. **Un aller-retour ne peut pas juger le format ; il
  ne juge que sa propre cohérence.** Il faut un invariant que l'écrivain
  n'emporte pas avec lui (ici : une ligne d'axe ne finit jamais par une espace)
  **et** un témoin qu'il n'a pas produit (un fichier écrit à la main dans le
  format d'avant).
- **Mutation F — `SetSizeMetric` garde le pointeur reçu au lieu de le copier :
  572/573.** Tous les noms des bancs étaient des **littéraux**, donc statiques,
  donc increvables. Il fallait un appelant dont le nom **meurt**. Et une fois ce
  contrôle écrit, **F survivait encore** : `NkString` garde inline tout texte de
  ≤ 23 caractères (`NK_STRING_SSO_SIZE`), donc dans le cadre de pile, que rien ne
  piétinait. **Il a fallu choisir un nom de 34 caractères pour que la faute ait
  lieu.**

> **Un banc de durée de vie doit choisir ses données pour que la faute AIT LIEU,
> pas seulement pour qu'elle soit possible.** Une chaîne courte, un seul
> internement, un nom littéral : trois façons différentes d'écrire un contrôle
> qui ne peut pas échouer.

⚠️ **Et un contrôle est tombé sur le code SAIN**, ce qui a corrigé une croyance :
« aucune ligne du fichier ne finit par une espace » est **faux** — `Field` écrit
`  <clé> = <valeur>`, donc tout champ vide (`composant`, `origine`, `ancrage`…)
finit déjà par une espace. L'invariant appartient à la **ligne d'axe**, pas au
fichier.

**Limite antérieure constatée au passage, sans rapport avec le pool** : un nœud à
~300 frères ne se relit pas — `Load` lit une ligne entière dans `val[256]` et la
ligne `enfants` déborde. Le comportement est celui qui est documenté : **refuser
plutôt que reconstruire à moitié**. Figé par un contrôle dans D5 pour que le jour
où la borne bouge, ça se dise.

### La correction (c) — appliquée le 2026-08-22

**Un repli qui préserve `success` n'est pas un repli, c'est un mensonge.**
`SerializeReflected` rendait `true` **inconditionnellement**, en ayant omis en
silence toute propriété qu'elle ne savait pas écrire — quatre sites, tous
commentés « ignoré ». Idem pour `DeserializeReflected`.

**Mesure d'impact faite AVANT la correction, pas après** : `19` appels de
`SerializeObject`/`DeserializeObject` dans tout le dépôt, dont **`0` en code
livré** (tous dans les bancs et les `tests/`), et **`1` seul** dont le retour
change — celui écrit exprès pour exposer la dette. Après correction : `0`
appelant sain cassé. `SandboxNKSerialization` 63/63, `SandboxNKArchive` 108/108,
`NKUIDesign` 36/36, NKECS compile.

⚠️ **LE VERDICT N'A PAS LA MÊME DÉFINITION DANS LES DEUX SENS.** C'est la
nuance que « ne mens pas sur le verdict » ne portait pas, et l'ignorer aurait
transformé la correction en **régression déguisée** : appliquer la règle
d'écriture au sens lecture aurait fait rendre `false` à **tout document
antérieur à son schéma**, cassant la compatibilité ascendante que R4/R5
protègent — et personne ne l'aurait vu avant de rouvrir un vieux fichier.

**Deux règles distinctes, et la différence est voulue :**

- **écriture** — toute propriété perdue rend `false`. On écrit quand même tout ce
  qu'on peut : une archive partielle vaut mieux que rien, du moment que le retour
  dit la vérité.
- **lecture** — une clé **absente** est **légitime** (champ optionnel, document
  d'une version antérieure) et ne rend pas `false` ; la traiter en erreur
  casserait la compatibilité ascendante. Seule une clé **présente et illisible**
  rend `false` : la donnée est dans le fichier et n'arrive pas dans l'objet.

Verrouillé par `wrote == false` dans C5, plus un témoin de non-régression sur un
type sain (`Point`) qui doit continuer à rendre `true`. **Mutation 8** — retour
de `WriteScalarProperty` de nouveau jeté : **62/63**, `SerializeObject -> true`,
le mensonge revient et le banc le voit.

**Trois réponses possibles, arbitrage ouvert** (il touche le kit, donc le
gardien de la forme) : (a) gérer `const char *` dans le pont — trivial en
écriture, impossible en lecture sans propriétaire ; (b) faire porter au document
un pool de chaînes dans lequel les `const char *` pointent ; (c) refuser
explicitement au lieu de retourner `true` — **FAITE le 2026-08-22**, voir la
section ci-dessus. (a) est **abandonnée** : triviale en écriture, impossible en
lecture sans propriétaire — une voie qui ne marche que dans un sens n'est pas une
voie. **(b) est la conception retenue**, et la réflexion non intrusive la rend
abordable.

### `NkArchive` porte la mise en forme d'origine — 2026-08-21

**Le manque, en une phrase.** `NkArchive` ne portait rien de ce qu'un format
texte éditable à la main doit rendre à l'octet. Trois choses, pas une :

| ce qui manquait | pourquoi ça comptait |
|---|---|
| commentaires et lignes vides | exigence explicite de Rodolf ; leur perte avait déjà été refusée une fois sur `.nkgui` v0.3 |
| **l'ordre du fichier** | l'archive est ordonnée par insertion, donc déterministe — mais l'ordre obtenu est celui de la **déclaration du schéma**. Dès qu'un aller-retour passe par l'archive, les propriétés sont réordonnées et « octet pour octet » tombe |
| la forme littérale | `0.50` contre `0.5`, la casse d'une couleur hexadécimale, les guillemets. Réécrire une valeur canoniquement, c'est modifier une ligne que l'utilisateur n'a pas touchée |

**Ce qui a été ajouté** — `NkArchiveTrivia` (`leading`, `trailing`, `literal`,
`literalOf`, `sourceOrder`), attaché **par pointeur possédant facultatif** à
`NkArchiveNode` et à `NkArchive`. Plus, sur l'archive : `SetLeadingTrivia` /
`SetTrailingTrivia` / `SetLiteral` / `SetSourceOrder` par clé,
`SetHeaderTrivia` / `SetFooterTrivia`, `Lexeme(key)`, `SortBySourceOrder()` et
surtout **`AdoptFormatting(source)`**, qui greffe la mise en forme d'une archive
lue sur une archive reconstruite depuis le modèle. C'est cette dernière qui rend
l'aller-retour possible en pratique.

**Trois garanties, et comment elles tiennent :**

1. **Additive.** Un nœud sans trivia n'alloue rien (8 octets de pointeur nul) et
   se comporte exactement comme avant. Mesuré : mêmes octets JSON et mêmes
   octets NKS1 pour une archive avec et sans trivia (banc, T5 et T6).
2. **Facultative en mémoire.** Un document fabriqué par le code n'a ni
   commentaire, ni ordre d'origine, ni forme littérale : c'est valide, et
   `Lexeme()` retombe alors sur la forme canonique.
3. **Le piège de `NkGValue::raw` fermé PAR CONSTRUCTION.** L'écrivain `.nkgui`
   réémet `raw` verbatim ; un document construit en mémoire a un `raw` vide et
   s'écrit donc **vide, sans erreur**. Ici c'est impossible : `Lexeme()` n'a pas
   de branche qui rende du vide par défaut d'information, et pour `NK_VALUE_NULL`
   la forme canonique est le mot-clé `null`. Second piège, plus vicieux : un
   littéral **périmé**. `literalOf` retient le texte canonique auquel le littéral
   correspondait ; une valeur éditée (0.50 → 0.75) **désarme son littéral toute
   seule**. Sans ce garde-fou, l'écrivain réimprimerait `0.50` — c'est-à-dire
   **perdrait la modification de l'utilisateur**.

⚠️ **`NkArchive` a cessé d'être `= default` sur ses cinq méthodes spéciales.**
Elle porte désormais un pointeur possédant ; un `= default` provoquerait une
double libération. Mesuré : la mutation qui partage le pointeur au lieu de le
dupliquer tue le banc par **corruption de tas** (`0xC0000374`), avant même la
première ligne de sortie.

**Preuve — `Sandbox/System/NKArchive`, application console, 105/105, sortie 0.**
Sous `Sandbox/` et pas sous `tests/`, pour la raison de la section précédente.
Le cas qui tranche est **T1** : un fichier dont les propriétés sont dans un ordre
différent de l'ordre de déclaration du schéma, avec un commentaire en fin de
ligne et un flottant écrit `0.50`, fait l'aller-retour complet — lecture,
modèle réfléchi, réécriture — **octet pour octet**. Et **T0** garde la trace du
problème : le même aller-retour avec un écrivain qui ignore la trivia **abîme**
le fichier, et le banc nomme les trois causes une par une. Si T0 devenait vert,
c'est que le banc ne mesurerait plus rien.

**Discrimination prouvée par cinq mutations du code testé** (`NkArchive.cpp`) :

| mutation | résultat |
|---|---|
| `SortBySourceOrder` neutralisée | 93/101 — T1, T2, T8 rouges |
| `Lexeme()` renvoie toujours la forme canonique | 93/101 — T1, T3, T7, T8 rouges |
| garde-fou anti-périmé désactivé | 95/101 — **T1 reste VERT** : un aller-retour sans édition ne peut pas révéler un littéral périmé. Seul un contrôle qui ÉDITE l'attrape |
| trivia partagée au lieu d'être dupliquée | **crash**, corruption de tas |
| commentaires non greffés par `AdoptFormatting` | 96/101 — T1, T3, T8 rouges |
| écrivain JSON rendant du vide (contrôle de l'ancrage de T5) | 102/105 — **seules les trois lignes d'ancrage rougissent**, `EXPECT_STREQ` reste vert |

⚠️ **Un contrôle et ce qu'il contrôle ne doivent jamais partager une cause.**
T5 violait cette règle et a été corrigé le 2026-08-21 (`cafac51b`) : il comparait
le JSON d'une archive **avec** trivia au JSON de la même archive **sans** trivia,
donc **deux sorties de la même fonction**. Un `NkJSONWriter::WriteArchive` qui
rendrait du vide aurait rendu les deux côtés vides — égalité vraie, contrôles
d'absence vrais, **T5 vert sur un écrivain JSON mort**. La parade est d'ancrer un
côté sur du texte **écrit à la main**, qui ne peut pas venir du code testé.
Mesuré par une sixième mutation : écrivain JSON rendant du vide → **102/105**, et
les trois seules lignes rouges sont les trois lignes d'ancrage, `EXPECT_STREQ`
restant vert. La même nuit, trois autres contrôles du dépôt sont tombés par ce
même défaut (une parade `grep` dont le commentaire contenait le motif compté ; un
banc où `0 == 0` passait sur un maillage jamais chargé ; un garde défensif qui
remplissait lui-même la condition qu'il testait).

**Limite nommée.** La trivia est portée par les nœuds et par l'archive ; elle ne
couvre donc pas ce qui n'est ni l'un ni l'autre — typiquement un commentaire
posé entre le nom d'une clé et son `=`. Aucun besoin connu à ce jour.

### L'indentation était la **quatrième** face du même manque — 2026-08-27

**Le manque.** La passe de trivia **jetait délibérément** l'indentation d'espace
pur devant chaque jeton, et l'écrivain la **régénérait** à
`profondeur × style.indent`, où `style.indent` est **une seule largeur déduite
pour tout le document**. Ce n'était pas une perte accidentelle : c'était une
**normalisation**, écrite en toutes lettres dans le code. Un fichier qui indente
partout pareil revenait à l'octet ; un fichier écrit à la main, non.

C'est la même phrase que les trois autres manques : **ce que le fichier disait,
et que le modèle n'a pas à savoir.**

> ⚠️ **Le corpus ne pouvait pas la montrer : il indentait partout pareil.** Un
> corpus de dix fichiers du même moule mesure le moule, pas le format. Le
> premier geste du correctif n'a donc pas été de coder — ç'a été d'écrire un
> fichier **volontairement irrégulier** (deux espaces ici, un tabulateur là, une
> ligne collée à la marge) et de le **voir rouge**. Sans lui, le témoin aurait
> été vert des deux côtés et n'aurait rien prouvé.

**La règle retenue** (arbitrage de Rodolf, option (b)) :

> **Une ligne qui vient du fichier garde ses octets ; une ligne créée par
> l'éditeur reçoit une indentation générée.**

#### ⚠️ Ce n'était pas deux edits. Et la mesure l'a dit avant le raisonnement.

Les deux edits « évidents » — garder l'espace dans la trivia, et donner à
`CloseLine` la même garde qu'à `OpenMember` — ont été appliqués, construits et
mesurés : **35 / 59**, et **aucun** des sept contrôles d'indentation ne passait
au vert. La sortie réémise tenait sur une ligne :

```
nkgui 0.3
widgets {  VBox "v" {        gap = 4  }
}
```

**Cause.** L'écrivain décidait « ce membre POURSUIT la ligne précédente » sur le
critère **« sa trivia ne contient pas de saut de ligne »**. C'est une
**reconstruction** d'un fait que le lecteur, lui, connaissait avec certitude.
Tant que l'espace pur était jeté, la reconstruction coïncidait avec la vérité.
Dès qu'on le garde, les deux cas deviennent **les mêmes octets** :

| bytes | ce que ça veut dire |
|---|---|
| l'espace entre `{` et `min` dans `Slider "n" { min = 0 }` | **sépare** deux jetons d'une même ligne |
| l'espace devant `gap` dans `  gap = 4` | **indente** une ligne nouvelle |

**Le correctif réel.** Le `lead` porte désormais **son propre terminateur de
ligne** : la passe de trivia coupe l'intervalle entre deux jetons en exactement
deux morceaux verbatim — `trail` (le reste de la ligne précédente, terminateur
exclu) et `lead` (**tout le reste**, terminateur compris) — et **n'en jette
aucun**. L'écrivain lit alors trois cas non ambigus :

| trivia de tête | signification | ce que fait l'écrivain |
|---|---|---|
| **vide** | le nœud est **neuf** — il ne vient d'aucun fichier | ferme la ligne et **génère** l'indentation |
| **sans saut de ligne** | séparateur sur la même ligne (`, `, l'espace d'un bloc en ligne) | réémet verbatim, n'indente pas |
| **avec saut de ligne** | elle porte elle-même la fin de la ligne précédente **et** l'indentation d'origine | **annule** le saut en attente au lieu de l'émettre, réémet verbatim, n'indente pas |

Le mécanisme vit à **deux endroits** — `OpenMember` pour un membre, `CloseLine`
pour une accolade fermante. Les deux ont été corrigés, et la mutation M2 mesure
ce que coûte de n'en corriger qu'un.

**Effet de bord acquis, et il vaut mieux que le correctif.** Le terminateur
n'est plus **déduit** de `style.crlf` (une déduction *globale*, prise sur la
première ligne du fichier) : il est **transporté**. Un document aux fins de
ligne mixtes n'est donc plus « réparé » en douce.

#### Le relevé

| banc | avant | après |
|---|---|---|
| NKUIDesign contrôles | 50 / 50 | **65 / 65** |
| corpus `valides/` | 7 / 7 à l'octet | **9 / 9 à l'octet** |
| corpus `limites/` | 0 / 1 à l'octet | **le dossier n'existe plus** |
| `SandboxNKArchive` | 352 / 352 | 352 / 352 |
| `SandboxNKSerialization` | 63 / 63 + 1 dette | 63 / 63 + 1 dette |
| sonde | 103 / 103 | 103 / 103 |
| pool | 573 / 573 | 573 / 573 |

Les acquis ont été **mesurés, pas supposés** : les corpus indentent uniformément,
donc ils *devaient* être intacts — la mesure le dit maintenant.

#### Les mutations — sept appliquées, sept tuées, et deux qui tranchent

| mutation du code testé | résultat |
|---|---|
| **M1** — `Indent()` n'émet plus rien | **60 / 61, et la seule ligne rouge est 24i** |
| **M2** — `CloseLine` reste sur l'ancienne règle (un seul des deux domiciles corrigé) | 34 / 61, corpus **0 / 7** |
| **M3** — l'ancienne normalisation rendue au `lead` (queue d'espaces coupée) | 37 / 61, corpus 0 / 7 |
| **M4** — le saut en attente **émis** au lieu d'être annulé (terminateur doublé) | 34 / 61, corpus 0 / 7 |
| **M5** — le `lead` démarre après le `\r` (terminateur CRLF perdu) | **59 / 61, et les seules lignes rouges sont 24j et 24k** |
| **M6** — la récursion du balayage retirée | 63 / 65, rouges 25 et 25b |
| **M7** — la validation reprend son appel non récursif (un seul geste corrigé) | 64 / 65, rouge 25b |

⚠️ **M1 est la plus instructive du lot.** Un écrivain qui **n'indenterait plus
jamais rien** passe **60 des 61 contrôles**, avec tout le corpus à 9 / 9 octet
pour octet. « Une ligne qui vient du fichier garde ses octets » est une moitié de
règle ; sans « une ligne créée par l'éditeur reçoit une indentation générée »,
elle est satisfaite par un écrivain mort. C'est **24i**, et lui seul, qui tient
l'autre moitié.

⚠️ **M5 dit ce que le corpus ne peut pas voir.** Le corpus est figé en LF
(`.gitattributes`, et pour une bonne raison : ses positions en octets font foi).
Le prix est qu'il est **entièrement aveugle au terminateur** : avec M5 appliquée,
les 20 fichiers restent 15 / 20 et 9 / 9, inchangés. Seuls les contrôles 24j
(CRLF) et 24k (fins de ligne mixtes) le voient. **Un corpus figé mesure ce qu'on
a figé.**

#### La récursion du balayage — et un contrôle mort trouvé par sa propre mutation

`--roundtrip` et `--valider` ne descendaient pas dans les sous-dossiers. Le geste
naturel — viser la racine du corpus — rendait « 0 erreur » pour quatorze fichiers
dont **pas un n'avait été ouvert**. Le message d'échec ajouté le 23/08 expliquait
comment contourner l'outil, dossier par dossier : **un diagnostic qui remplace
une capacité est une dette, pas une parade.** La collecte descend désormais, et
elle vit à **un seul endroit** (`NkGCollecter`) parce que les deux gestes en
portaient deux copies, message d'échec compris.

Mesure : la racine du corpus passe de **0** fichier lu à **20**.

⚠️ **Le contrôle 25b était mort à sa première écriture, et c'est M7 qui l'a dit.**
Il exigeait `rt == 0 && vd == 0` — **le succès des deux gestes**. Or un balayage
qui lit **moins** réussit tout autant : M7 est passée à **65 / 65**. C'est la
famille exacte des deux `Find(...) == npos` qui rendaient T5 vert sur un écrivain
JSON mort — **exiger un succès, c'est être satisfait par le vide**. Le contrôle
est désormais ancré sur un **refus** : un fichier délibérément illisible est
enterré à trois niveaux, et les deux gestes doivent chacun rendre `1`. Un
balayage qui ne descend pas rend `0`, un balayage qui ne lit rien rend `2` :
**seul un balayage qui a vraiment atteint le fond rend `1`.**


### `appearance(État)` cesse d'être une tranche verbatim — 2026-08-27

**Le manque.** `LooksLikeBlock` n'acceptait que `Ident [Str] {`. `appearance(Hover) {`
tombait donc dans `Raw` — une tranche de source conservée telle quelle. Le fichier
revenait à l'octet (**une tranche verbatim revient toujours à l'octet**) et son
contenu n'était jamais jugé.

> **La partie modélisée crie, la partie non modélisée se tait.** La même faute
> était vue dans `appearance` et se taisait dans `appearance(Hover)`. Mesuré, pas
> supposé : même fichier, même faute, `1 erreur` d'un côté et `0` de l'autre.

**Ce qui a débloqué** : la liste fermée des états, tranchée par Rodolf —
`Normal · Hover · Pressed · Focus · Disabled` — après un relevé de huit outils.

**Ce qui a été ajouté** : une clé réservée `$state`, son littéral portant la
parenthèse entière telle qu'écrite, et `StateOf()`.

#### ⚠️ La question que `Normal` a ouverte, et que la promesse a tranchée seule

`appearance { }` et `appearance(Normal) { }` désignent-ils la même chose ?
**Oui — synonymes.** Chez tous les outils qui *nomment* le repos (Unity, Godot,
WPF, Figma), **le repos nommé EST le socle** ; aucun n'a à la fois un socle et un
repos distincts, et CSS n'a même pas de `:normal`.

Mais deux graphies pour un sens, dans un format dont la promesse est l'octet, ne
tiennent qu'à une condition : **le modèle ne canonise pas.** `$state` est
*absente* quand le fichier n'écrit pas de parenthèses, et l'écrivain réémet le
lexème — espaces intérieurs compris. Sans ça, il faudrait en régénérer une, et
tous les documents employant l'autre cesseraient de revenir à l'octet. **Même
mécanisme que `0.50` contre `0.5`, et c'est bien le même : `literal`.**

Conséquence qui avait besoin de son diagnostic : les cumuler déclare deux fois le
même état → `W-ÉTAT-DOUBLE`. **Avertissement et non erreur**, délibérément — dire
laquelle gagne trancherait la question de la *combinaison* d'états, qui reste
ouverte et ne nous appartient pas.

#### ⚠️ Le contrôle 20 a rattrapé une erreur de conception, pas une régression

La première version acceptait `Ident ( Ident ) {` **partout**. `futurMembre(x) { }`
— le membre inconnu d'un fichier 0.4 — cessait alors d'être conservé verbatim :
**on inventait une structure pour une construction qu'on ne connaît pas**, puis on
jugeait son contenu contre un schéma qu'on n'a pas. **La règle (d) sacrifiée pour
fermer une limite.** Le document 9 §7 est net : seul `appearance` porte un
`(État)`. D'où `HeadTakesState()`, un prédicat nommé dont la raison est écrite
au-dessus.

#### Le relevé

| banc | avant | après |
|---|---|---|
| NKUIDesign contrôles | 66 / 66 | **73 / 73** |
| corpus `valides/` | 9 / 9 à l'octet | **10 / 10 à l'octet** |
| `SandboxNKArchive` | 352 / 352 | 352 / 352 |
| `SandboxNKSerialization` | 63 / 63 + 1 dette | 63 / 63 + 1 dette |
| sonde | 103 / 103 | 103 / 103 |
| pool | 573 / 573 | 573 / 573 |

Le contrôle **23e a été retourné** : il figeait l'asymétrie (« 1 diagnostic, pas
2 ») et exige désormais **2** diagnostics, chacun avec sa ligne *et son état dans
le chemin*. Même fichier, même faute, chiffre inverse.

#### Cinq mutations, cinq tuées — et chacune ne touche qu'un contrôle ou deux

| mutation du code testé | résultat |
|---|---|
| **N1** — la liste accepte tout | 71 / 73, rouges **26b et 26c** (26a reste vert : « 0 diagnostic » est une absence) |
| **N2** — le doublon n'est plus signalé | 71 / 73, rouges 26d et 26e |
| **N3** — `appearance` nu n'est plus `Normal` | 72 / 73, rouge **26d seul** |
| **N4** — l'écrivain **régénère** l'état au lieu de réémettre son lexème | 72 / 73, rouge **26f seul** |
| **N5** — l'en-tête parenthésé redevient générique | 72 / 73, rouge **20 seul** |

⚠️ **N4 est celle qui comptait.** C'est exactement le mode de rupture prévu — « si
l'écrivain n'en régénère qu'une, l'aller-retour casse pour les documents qui
emploient l'autre » — et **un seul contrôle du banc le voit**. Sans 26f, la
promesse d'octet reposait sur un commentaire.

⚠️ **N1 redit la leçon du matin** : 26a (« les cinq passent, 0 diagnostic ») est
une **absence**, donc satisfaite par une liste qui accepterait tout. Ce sont 26b
et 26c — deux contrôles de **refus** — qui la tiennent.


### Tests — suite standalone
[test_smoke.cpp](tests/test_smoke.cpp) — 15 tests sans framework externe :
1. Archive flat (Set/Get scalars, Remove, Has).
2. Archive hierarchical (SetObject/GetObject + array de scalars).
3. Archive path (`a.b.c` deep set/get) + metadata.
4. JSON round-trip flat.
5. JSON round-trip nested.
6. XML round-trip.
7. YAML round-trip.
8. Binary round-trip.
9. NkNative round-trip flat (`0xDEADBEEFCAFEBABE`).
10. NkNative nested objects.
11. NkAssetId generate/toString/fromString (35 chars `{16-16}`).
12. NkAssetMetadata serialize/deserialize (tags + deps + properties).
13. NkAssetIO write/read fichier `.nkasset` (2KB payload).
14. NkAssetRegistry save/load registry + lookups.
15. Archive Merge (overwrite=true|false).

### Outils
- `cli/tools/nkecs-convert` : utilitaire de conversion (probablement entre
  formats ou pour ECS — auditer en détail si besoin).

### Quirks réglés
- `NkAssetMetadata.h` undefines `GetObject` Win32 macro pour éviter collision
  avec `NkArchive::GetObject` quand `<windows.h>` est inclus en amont.

---

## En cours / TODO immédiat

### Câblage formats projet PV3DE / Noge (priorité 1)
ARCHITECTURE.md §8 demande explicitement :
- `.nkproj` (projet Noge) : JSON via `SaveToFile<NkProject>(path, NK_JSON)`.
- `.nkscene` (scène ECS) : JSON, sérialisation de `NkScene` (entities +
  components réfléchies).
- `.nkcase` (cas clinique PV3DE) : JSON étendu, sérialisation de
  `NkCaseLoader` data (cf. §5.3 NKDiagnostic).
- `.nkb` (assets binaires compilés) : NkNative + payload binaire.

Aujourd'hui les extensions ne sont **pas reconnues** par
`DetectFormatFromExtension` (seuls `json/xml/yaml/yml/bin/nk/nkasset` le sont).
Ajouter `nkproj → NK_JSON`, `nkscene → NK_JSON`, `nkcase → NK_JSON`,
`nkb → NK_NATIVE`.

### Intégration NKReflection runtime (priorité 1)
`NkReflect.h` (Native/) est **statique compile-time** (macros). Pour
InspectorPanel et édition live, il faut **runtime reflection** via
NKReflection :
- `NkSerializeReflect<T>(writer, instance)` itère sur `NkClass::GetProperty(i)`
  et appelle le sérialiseur générique selon `NkTypeCategory`.
- Bidirectionnel : `NkDeserializeReflect<T>` symétrique.
- Skip `NK_TRANSIENT` flag.
- Dépendance : NKReflection (voir leur ROADMAP — bloque ici).

### NkSchemaVersioning à finir
[NkSchemaVersioning](src/NKSerialization/NkSchemaVersioning.h) existe.
À durcir :
- Champ `__version__` auto dans tout NkArchive racine.
- Pipeline de migration `Migrate(archive, fromVersion, toVersion)` avec
  registry de migrateurs par type.
- Skipping/defaulting des champs ajoutés/retirés.
- Tests dédiés.

### Tests à étendre
- Tester `Serialize<MyClass>(obj, fmt, str)` end-to-end via
  `NkISerializable`, pas seulement `NkArchive`.
- Tester la factory polymorphique `CreateFromArchive` avec un type
  enregistré.
- Tester `NkReflect` macros statiques.
- Tester le détecteur magic bytes sur chaque format.
- Benchmark : décommenter `NkSerializationBenchmark.h` et publier des
  baselines (taille comparée JSON vs Binary vs Native, temps de serialize +
  deserialize, throughput MB/s par format).

---

## À venir / À ajouter (futur proche)

### Hot-reload des assets
- Couplage `NkFileWatcher` (NKFileSystem) + `NkAssetRegistry` : détection
  modification → rechargement automatique → notification consommateurs
  (`onAssetReloaded(NkAssetId)`).
- Cas d'usage Noge : modifier un `.nkasset` matériau → renderer recharge à
  la volée (cf. NKRenderer Phase G material hot-reload déjà livré).

### Compression dans NkNative
- Flag dans le file header : `COMPRESSED_LZ4` / `COMPRESSED_ZSTD`.
- Compress at write, decompress at read.
- Bénéfice immédiat sur `.nkb` (meshes, textures) et archives volumineuses.

### Streaming partiel
- API `NkAssetIO::ReadMetadataLazy(path)` qui ne lit que le header sans
  charger le payload — utile pour AssetBrowser thumbnails sans I/O complet.
- Streaming par chunk pour les gros assets (animation clips, audio long).

### NkAssetImporter pipeline complet
- Plugins d'import par type source : `.fbx`/`.gltf` → mesh, `.png`/`.exr` →
  texture, `.wav`/`.ogg` → audio.
- Dépendances inter-asset détectées à l'import.
- Generation des thumbnails (PNG 256x256) stockés dans le `.nkasset` payload
  ou en sidecar.

### Conteneurs réflechis et types custom
- Sérialiseurs natifs pour `NkVec3f`, `NkMat4f`, `NkQuat` (NKMath).
- Sérialiseurs pour les types NKTime (`NkDate`, `NkTimeSpan`).
- Pattern d'extension : `template<> void NkSerialize<MyType>(...)` spécialisé.

### CLI tools
- Étendre `nkecs-convert` : commandes `nk-validate <file>`, `nk-inspect <file>
  --json`, `nk-pack <dir> --out=archive.nkb`.
- Diff sémantique de deux archives (utile pour Git merge `.nkscene`).

### Sécurité / sanity
- Vérifier les CRC32 systématiquement à la lecture NkNative.
- Limites de profondeur de récursion (anti-DoS sur archive malicieuse).
- Bounds checking strict sur le parser binaire.

---

## Bugs / quirks connus
- Le `static_assert` de `Deserialize<T>` utilise `std::is_base_of` alors que
  `LoadFromFile<T>` utilise `traits::NkIsBaseOf` — incohérence stylistique
  (les deux fonctionnent mais le projet vise STL-free).
- `NkSerializationBenchmark.h` est commenté dans l'umbrella header. Ré-enable
  optionnel via macro à exposer.
- Doublon de commentaire doctring en haut de `NkISerializable.h` (le bloc est
  répété deux fois).
- Tests utilisent `/tmp/` en path littéral — non portable Windows. Devrait
  passer par `NkFileSystem` pour un tmp cross-platform.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (Types, Traits, Assert),
  NKContainers (NkString, NkStringView, NkVector), NKFileSystem (NkFile,
  NkFileMode), NKMemory (NkUniquePtr).
- **Couches en dessous optionnelles** : NKReflection (pour
  `NkSerializeReflect<T>` runtime — pas encore câblé).
- **Modules au-dessus qui en dépendent** :
  - **Noge / ProjectManager** : I/O `.nkproj`, `.nkscene`
    (cf. ARCHITECTURE.md §4.3).
  - **Noge / AssetManager** : `.nkasset` registry, hot-reload.
  - **NKRenderer / NkMaterialAsset** : déjà livré, charge `.nkasset` JSON
    pour les matériaux (cf. NKRenderer ROADMAP Phase G).
  - **NKScene** : sérialisation entities + components (via NKReflection une
    fois câblée).
  - **PV3DE / NkCaseLoader** : chargement `.nkcase` (JSON étendu) pour les
    scenarios cliniques (cf. ARCHITECTURE.md §5.3).
  - **PV3DE / ReportPanel** : export rapport FHIR (JSON) /
    PDF (cf. ARCHITECTURE.md §5.10).
