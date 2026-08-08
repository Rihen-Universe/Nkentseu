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
procédurale, machine à états d'animation, motion) **restent ouverts jusqu'à leur
chantier** : un nom choisi avant de savoir ce que le fichier contiendra vraiment
est précisément le nom qu'on regrette.

Deux repères pour ce jour-là, tirés de l'état de l'art :
- **l'animation** — Unity (Animator Controller) et Unreal (Animation Blueprint)
  mettent **états ET mélangeurs dans un seul fichier**, parce qu'une machine à
  états *contient* des mélangeurs dans ses états ; les séparer forcerait deux
  fichiers pour une logique indivisible. Un clip reste `.nkanim`, distinct ;
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
| `Sound` (7) | **`.nksnd`** | son compilé |
| `Font` (14) | **`.nkfont`** | police compilée (atlas + métriques) |
| `Shader` (15) | **`.nkshader`** | shader compilé |
| `Blueprint` (9) | **`.nkbp`** | graphe de script visuel |
| `Prefab` (13) | **`.nkprefab`** | assemblage réutilisable |
| `DataTable` (10) | **`.nkdata`** | table de données |
| `Map` (11) / `World` (12) | **`.nkmap`** / **`.nkworld`** | niveau / monde |
| `Script` (16) | **`.nkscript`** | script |
| `Custom` (255) | **`.nkasset`** | nature non standard |

**`.nkasset` reste accepté EN LECTURE** (compatibilité avec l'existant), mais
n'est **plus écrit** — sauf pour `Custom`, dont c'est justement la nature.

### Distinct des formats de PROJET (déjà dans `ARCHITECTURE.md` §8)

`.nkproj` (projet) · `.nkscene` (scène) · `.nkcase` · `.nkb` (binaire compilé).
Ceux-là sont du JSON ou un binaire propre, **pas** le format asset ci-dessus.

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
sans ouvrir*. Un projet de NK3DModeler et un projet de NKCode n'ouvrent pas le
même logiciel ; leur donner la même extension obligerait à lire le fichier
pour savoir quoi en faire, et empêcherait l'association par double-clic.
`.nkproj` reste le nom **générique** de `ARCHITECTURE.md` pour les projets
d'autres applications.

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
