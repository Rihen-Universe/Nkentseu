# Conventions de fichiers Nkentseu — extensions, projet, assets

> **Décision de Rihen, 5 août 2026.** Document de référence, **suivi par git**
> (contrairement aux fichiers `.private.md`) : il doit survivre à un clone neuf
> et à toute session perdue. Lié depuis `CLAUDE.md`.

## 1. La règle : un seul format, une extension par nature

Tous les assets Nkentseu partagent **un seul et même format binaire** :

```
en-tête NkAssetFileHeader (40 o)  +  NkAssetMetadata (NkNative)  +  payload
```

Ce format porte **déjà** sa nature dans son en-tête (`NkAssetType`, cf.
`Kernel/System/NKSerialization/src/NKSerialization/Asset/NkAssetMetadata.h:204`).
Mais un dossier de deux cents `.nkasset` est **illisible** : rien ne distingue
un matériau d'une texture sans ouvrir chaque fichier.

**Donc : une extension par nature, structure identique.** L'extension se trie à
l'œil, se filtre dans un navigateur, porte une icône — sans ouvrir le fichier.

### L'extension est un INDICE, l'en-tête est la VÉRITÉ

Un lecteur qui ouvre un `.nkmat` dont l'en-tête annonce une texture **suit
l'en-tête** et journalise la discordance. Jamais l'inverse. Un fichier renommé
à la main reste donc lisible et se signale — il ne corrompt rien.

### L'extension dit ce que le fichier PRODUIT, jamais comment il est écrit dedans

Question de Rihen (6 août 2026) : les matériaux **nodaux** méritent-ils une
extension à eux ? Non — et la raison vaut pour tous les cas à venir.

**Le nodal n'est pas un type d'asset, c'est une façon de décrire le même asset.**
Un matériau réglé par curseurs et un matériau construit par nœuds contiennent la
même chose (une description de surface), s'appliquent au même endroit (un
maillage) et s'ouvrent dans le même éditeur. `.nkmat` et `.nkmatnode` seraient
**deux fichiers pour une seule idée** : l'utilisateur devrait savoir *comment* un
matériau est écrit avant de pouvoir s'en servir.

Mais Rihen a raison sur le fond : **tous les graphes ne se valent pas**. Un
graphe de matériau, un graphe de géométrie et un graphe de motion ne produisent
pas la même chose, et leurs nœuds ne sont pas interchangeables (« extruder » n'a
aucun sens dans un matériau ; « mélanger deux BSDF » n'en a aucun dans une
animation). La règle n'est donc **pas** « un graphe = une extension » :

> **Deux fichiers qu'on peut déposer au même endroit avec le même effet
> partagent leur extension. Sinon, ils en changent.**

Déposer un matériau sur un objet change son **apparence** ; déposer un graphe de
géométrie change sa **forme**. Deux gestes, deux résultats, deux extensions —
alors qu'un matériau nodal et un matériau simple, déposés au même endroit, font
strictement la même chose.

**Corollaire — la simplicité ne vient pas du NOMBRE d'extensions mais de leur
PRÉVISIBILITÉ.** Cinq extensions répondant chacune à « ça s'applique à quoi ? »
sont simples ; deux extensions dont l'une signifie tantôt une chose tantôt une
autre sont compliquées. Le risque vient toujours du même endroit : une extension
qui décrit **l'implémentation** au lieu de **l'usage**.

**Recette ≠ résultat.** `.nkmesh` contient des sommets (une donnée) ; un graphe
de géométrie contient une recette qui *produit* des sommets (un programme). Même
distinction qu'entre une texture cuite et une texture procédurale. Confondre les
deux, c'est confondre une photo et l'appareil photo.

### Aucun nom n'est gravé avant le premier octet écrit

Les extensions décidées ici — `.nkmat`, `.nkmati`, `.nk3dm` — le sont parce que
**ces formats s'écrivent maintenant**. Les noms des formats à venir (géométrie
procédurale, motion) **restent ouverts jusqu'à leur chantier** : un nom choisi
avant de savoir ce que le fichier contiendra vraiment est précisément le nom
qu'on regrette.

**La machine à états d'animation a eu son chantier, et son nom** (décision de
Rihen, 30 septembre 2026) : **`.nkanimctl`**, « contrôleur d'animation ».
L'extension dit l'**usage** — ce qui pilote l'animation d'un personnage — et
non l'implémentation (« hfsm », « graph ») ; et c'est la règle ci-dessus
appliquée : un clip et un contrôleur ne se déposent pas au même endroit avec le
même effet (l'un est **joué**, l'autre **choisit** ce qui est joué), donc ils
ne partagent pas d'extension. Pendant un jour (29 → 30/09), la machine s'est
écrite en `.nkanim` v3 : ces fichiers se relisent toujours comme machine, et un
lecteur de clip les refuse en le disant.

Deux repères pour ce jour-là, tirés de l'état de l'art :
- **l'animation** — Unity (Animator Controller) et Unreal (Animation Blueprint)
  mettent **états ET mélangeurs dans un seul fichier**, parce qu'une machine à
  états *contient* des mélangeurs dans ses états ; les séparer forcerait deux
  fichiers pour une logique indivisible. Un clip reste `.nkanim`, distinct ;
  → appliqué le 30/09 : `.nkanimctl` porte les états, la hiérarchie, les
  transitions et les paramètres. ⚠️ Limite assumée : les clips et les blend
  trees y sont désignés par leur **nom** (ils appartiennent à l'appelant, qui
  les retrouve au chargement), pas recopiés dedans ;
- **la texture procédurale** ne mérite pas d'extension propre : elle produit une
  image *à l'intérieur* d'un matériau. Réutilisable seule, elle devient un
  matériau sans sortie de surface — pas un nouveau format.

## 2. La table

| `NkAssetType` | Extension | Contenu |
|---|---|---|
| `Material` (5) | **`.nkmat`** | matériau (modèle de surface + paramètres) |
| `MaterialInstance` (6) | **`.nkmati`** | instance : un matériau + ses surcharges |
| `Texture2D` (3) | **`.nktex`** | texture 2D compilée |
| `TextureCube` (4) | **`.nktexc`** | cubemap (ciel, IBL) |
| `StaticMesh` (1) | **`.nkmesh`** | maillage statique |
| `SkeletalMesh` (2) | **`.nkskel`** | maillage à squelette |
| `Animation` (8) | **`.nkanim`** | clip d'animation |
| `AnimationController` (17) | **`.nkanimctl`** | contrôleur d'animation : machine à états (états, sous-machines, transitions, paramètres) — décision de Rihen du 30/09/2026 |
| `Sound` (7) | **`.nksnd`** | son compilé |
| `Font` (14) | **`.nkfont`** | police compilée (atlas + métriques) |
| `Shader` (15) | **`.nkshader`** | shader compilé |
| `Blueprint` (9) | **`.nkbp`** | graphe de script visuel |
| `Prefab` (13) | **`.nkprefab`** | assemblage réutilisable |
| `DataTable` (10) | **`.nkdata`** | table de données |
| `Map` (11) / `World` (12) | **`.nkmap`** / **`.nkworld`** | niveau / monde |
| `Script` (16) | **`.nkscript`** | script |
| `Scene` (18) | **`.nkscene`** | une scène (entités, hiérarchie, état de la simulation) — ajouté le 2026-09-29 (17 à l'origine, passé à 18 à la fusion : `AnimationController` avait pris 17 ; aucun fichier n'écrivait ce numéro) |
| `SaveGame` (19) | **`.nksave`** | une sauvegarde de PARTIE — ajouté le 2026-09-29 |
| `Custom` (255) | **`.nkasset`** | nature non standard |

**`Scene` et `SaveGame` (2026-09-29).** `.nkscene` était déjà l'extension des scènes
d'Unkeny (JSON « `unkeny.scene` »), sans type d'asset pour la dire : un navigateur
ne pouvait ni l'icôner ni la filtrer par la table. Une **sauvegarde de partie** n'est
pas une scène qu'on édite — on ne la dépose pas au même endroit, on ne l'ouvre pas
dans le même outil — d'où sa propre extension (règle du § 1). Un `.nkscene` ou un
`.nksave` écrit en JSON porte sa nature dans son champ `format`, comme un asset
binaire la porte dans son en-tête : c'est lui la vérité.

**`.nkasset` reste accepté EN LECTURE** (compatibilité avec l'existant), mais
n'est **plus écrit** — sauf pour `Custom`, dont c'est justement la nature.

⚠️ **`.nkanim` et `.nkanimctl` ne suivent pas (encore) le format commun** : ils
ont leur propre en-tête, magic `NKAN` pour un clip, `NKAC` pour un contrôleur,
parce que NKAnima ne tire pas NKSerialization. La règle « l'en-tête est la
vérité » vaut pour eux par leur magic ; leur ligne de la table ci-dessus est,
elle, la seule correspondance type ↔ extension.

### Distinct des formats de PROJET (déjà dans `ARCHITECTURE.md` §8)

`.nkproj` (projet) · `.nkscene` (scène) · `.nkcase` · `.nkb` (binaire compilé).
Ceux-là sont du JSON ou un binaire propre, **pas** le format asset ci-dessus.
⚠️ Les extensions de projet et de scène de la famille sont redécidées au **§ 6**
(01/10/2026).

## 3. Le point de passage unique

La correspondance type ↔ extension vit **à un seul endroit**, dans
NKSerialization (là où vit le format), sous la forme de deux fonctions
inverses :

```cpp
const char *NkAssetExtensionFor(NkAssetType t) noexcept;   // Material -> "nkmat"
NkAssetType NkAssetTypeFromExtension(const char *ext) noexcept; // "nkmat" -> Material
```

**Ne jamais recopier la table ailleurs.** Une seconde copie diverge au premier
ajout — c'est la leçon déjà payée trois fois dans ce dépôt (les combos de la
sortie, `kVidExt` figé à 3, `kHdrNames` resté à six entrées).

## 4. Ce que ça implique, à faire

- [ ] `NkAssetExtensionFor` / `NkAssetTypeFromExtension` dans NKSerialization.
- [ ] `NkAssetIO::Write` choisit l'extension depuis le type (et corrige le
      chemin si l'appelant en a donné une autre, en le journalisant).
- [ ] `NkAssetIO::Read` accepte **toute** extension : c'est l'en-tête qui
      tranche ; discordance = avertissement, pas erreur.
- [ ] `DetectFormatFromExtension` (NKSerialization) reconnaît enfin les
      extensions du projet — **bug connu depuis l'audit du 26 mai 2026**.
- [ ] Le navigateur de projet du modeleur associe icône et filtre par
      extension.

## 5. Structure d'un projet (décision du 5 août 2026)

Un projet = **un dossier** + un fichier **`.nk3dm`** à sa racine.

```
MonProjet/
└── MonProjet.nk3dm       ← le projet du modeleur (JSON)
    …et ce que l'utilisateur range comme il veut
```

**Pourquoi `.nk3dm` et pas `.nkproj`** (arbitrage du 5 août, Rihen laissait le
choix) : c'est la règle de ce document appliquée à elle-même — *identifiable
sans ouvrir*. Un projet de NKCraft et un projet de NKCode n'ouvrent pas le
même logiciel ; leur donner la même extension obligerait à lire le fichier
pour savoir quoi en faire, et empêcherait l'association par double-clic.
`.nkproj` reste le nom **générique** de `ARCHITECTURE.md` pour les projets
d'autres applications.

**L'extension ne suit pas le renommage de l'application** (29/09/2026 :
NK3DModeler s'appelle désormais NKCraft). `.nk3dm` est un **format**, pas le nom
du produit : le changer rendrait chaque projet existant méconnaissable à
l'œil, casserait l'association par double-clic, et n'apporterait rien — la
règle « identifiable sans ouvrir » est tenue par l'extension, quelle que soit la
façon dont l'application s'appelle. Le champ `"application"` du JSON, lui,
passe de `"NK3DModeler"` à `"NKCraft"` ; il est informatif, aucun lecteur ne le
teste, et un lecteur futur doit accepter les deux.

### Import : COPIER, en gardant l'origine

Un fichier importé est **copié dans le projet** — un projet doit pouvoir être
déplacé, copié ou envoyé entier et continuer de fonctionner. Le chemin source
d'origine est **mémorisé dans les métadonnées de l'asset**, à une seule fin :
proposer de **réactualiser** l'import si le fichier d'origine a changé. Ce
n'est pas un lien vivant : la copie fait foi, et un original disparu ne casse
rien.

**Aucune arborescence n'est imposée** (précision de Rihen, 5 août) : le
modeleur a **déjà** son navigateur de projet, où l'on crée dossiers et
fichiers, y compris au moment de l'import. Lui imposer `Materials/`,
`Textures/`, `Meshes/`… ferait deux organisations concurrentes — celle du
logiciel et celle de l'utilisateur — et la sienne perdrait.

Ce qui est **obligatoire**, en revanche :

1. **Tous les chemins stockés sont RELATIFS au dossier du projet.** Un projet
   déplacé, copié ou envoyé à quelqu'un continue de fonctionner — c'est la
   raison d'être du dossier.
2. Un asset **peut vivre n'importe où** sous la racine du projet ; c'est le
   navigateur qui le retrouve, pas une convention de nom de dossier.
3. Une **destination par défaut** est proposée à l'import (le dossier courant
   du navigateur), jamais imposée.

## 6. Les extensions de la famille : projets et scènes (décision du 1er octobre 2026)

> Rihen, 01/10/2026 : *« avoir deux applications qui portent le même nom pour les
> projets n'est pas forcément la meilleure chose, sauf si ces dernières partagent les
> mêmes ressources ; exemple : les scènes de NKCraft sont les mêmes, avec les nuances
> pour chacune des applications NkAnimaEditor, Nogee, NKCraft, NKScena et PV3DE. »*
> Il a validé l'analyse ci-dessous le jour même.

### Le constat (mesuré dans le code le 01/10)

`.nkscene` porte aujourd'hui **quatre** sens : la scène 2D d'Unkeny (JSON
`unkeny.scene`), la scène 3D de NKCraft (avec sa géométrie sœur `.nkgeo`), la scène 3D
de NogeDemo (JSON), et le « document de scène » de Genia (la spécification d'une
créature avant sa géométrie, `Tools/Genia/FORMAT_SCENE.md`). Les projets ont, eux,
une extension par application : `.nk3dm` (NKCraft, § 5), `.nkproj` (Nogee),
`.nkprojet` (UnkenyEditor, alors que la décision Q4 du document 01 des scripts dit
`.nkunk`).

### La règle

**Une extension = un seul sens.** Deux applications partagent une extension
**seulement si elles lisent le même fichier de la même façon** — c'est-à-dire quand
elles partagent les ressources. C'est aussi ce que demande le système : une extension
ouvre une application par défaut.

### La table

| nature | extension | lu et écrit par | remplace (lu pour toujours) |
| --- | --- | --- | --- |
| projet 2D (un dossier de jeu Unkeny) | **`.nkunk`** | UnkenyEditor, UnkenyPlayer | `.nkprojet` |
| projet 3D (un dossier de la famille Noge) | **`.nknoge`** | Nogee, NKCraft, NkAnimaEditor, NKScena, PV3DE | `.nkproj` (Nogee), `.nk3dm` (NKCraft) |
| scène 2D | **`.nkscene2d`** | Unkeny | `.nkscene` au format `unkeny.scene` |
| scène 3D | **`.nkscene3d`** | les cinq applications 3D | `.nkscene` de NKCraft et de NogeDemo |
| géométrie d'une scène ou d'un maillage | `.nkgeo` (inchangé) | NKCraft, Noge | — |
| document de scène de Genia | **à renommer** (proposition : `.nkspec`, à confirmer par Rihen) | NKCraft / Genia | `.nkscene` de `Tools/Genia/` |
| séquence | `.nkseq` (inchangé ; **v2 le 02/10** : elle POINTE vers sa scène `.nkscene3d` et nomme ses cibles, la v1 reste lue) | NKScena | — |
| cas clinique | `.nkcase` (inchangé) | PV3DE | — |
| interface | `.nkgui` (inchangé) | toutes | — |
| assets (§ 2) | inchangés | toutes | — |

Un projet porte le nom du **moteur**, pas de l'application : un même jeu 3D s'ouvre
dans Nogee, NKCraft, NkAnimaEditor, NKScena ou PV3DE, parce qu'ils partagent ses
ressources. Ceci remplace, pour la famille 3D, l'arbitrage du 5 août (§ 5 : « un
projet = un `.nk3dm` ») ; les règles d'import et de chemins relatifs du § 5 restent.

### Les nuances de chaque application

1. Une scène 3D a une partie **commune** (entités, hiérarchie, transformations,
   maillages, matériaux, lumières, caméras) et des **sections nommées par
   application** : `nkcraft` (historique de modélisation), `nkanimaeditor` (rigs,
   clips en cours), `nkscena` (plans, pistes), `pv3de` (paramètres du cas), `nogee`
   (jeu, scripts).
2. **Une application garde intactes les sections qu'elle ne connaît pas.** Un banc
   le prouve : aller-retour NKCraft → Nogee → NKCraft sans rien perdre.
3. Les préférences propres à une application vivent dans un sous-dossier caché du
   projet (`.nkcraft/`, `.nogee/`…), jamais dans le fichier partagé.
4. Le champ `format` du JSON reste la vérité (§ 1) ; l'en-tête note aussi
   l'**application d'origine**.

### Le double-clic

`.nkunk` et `.nkscene2d` ouvrent UnkenyEditor. Pour `.nknoge` et `.nkscene3d`,
partagés, le **lanceur de la famille** ouvre l'application d'origine notée dans
l'en-tête, et « Ouvrir avec… » propose les autres.

### La bascule, sans rien casser

- Les anciennes extensions restent **lues pour toujours** (reconnues à leur contenu).
- Les nouvelles ne sont écrites qu'à l'enregistrement.
- Une application à la fois, chacune avec son banc ; Unkeny d'abord (`.nkunk`,
  `.nkscene2d`), puis la scène 3D partagée, puis les projets 3D.
- À faire dans NKSerialization : le type `Scene` (18) devient la scène 2D
  (`.nkscene2d`) et un type `Scene3D` est ajouté pour `.nkscene3d`, dans le point de
  passage unique du § 3.
