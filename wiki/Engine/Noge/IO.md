# L'import/export 3D & 2D (FBX · glTF · OBJ · SVG)

> Couche **Engine** · Noge · Faire entrer et sortir des **assets** : maillages, squelettes,
> animations, matériaux et dessins vectoriels, via les quatre formats du monde réel —
> FBX (Autodesk), glTF/GLB (le standard moderne), OBJ (le pivot universel) et SVG (le 2D).

Un moteur ne crée pas ses contenus tout seul : les artistes les fabriquent ailleurs — Blender,
Maya, 3ds Max, Substance, Illustrator — et les exportent dans un **format de fichier**. Le rôle de
la famille IO de Noge est ce **pont** : lire un `.fbx` ou un `.glb` et en sortir des maillages
prêts pour l'ECS, ou inversement écrire vos maillages dans un fichier qu'un autre logiciel saura
relire. La question n'est jamais « quel module marche » mais « quel format pour quel besoin » : FBX
pour récupérer du legacy Autodesk, **glTF en priorité** pour le pipeline moderne (le format que le
moteur préfère), OBJ comme dénominateur commun robuste, SVG pour le design vectoriel 2D.

Avant tout, une mise en garde nécessaire. Ces quatre modules sont **des en-têtes de spécification** :
aucune méthode n'y a de corps réel (seuls quelques getters triviaux comme `GetLastError`/`IsValid`
sont inline). Les commentaires glTF emploient ✅/⚠️/❌, mais ce sont des **cibles déclarées**, pas
des preuves d'implémentation — la note projet rappelle que les sous-systèmes Modeling/Anim/Design de
Noge sont largement « des headers sans `.cpp` ». Traitez donc cette page comme la documentation d'une
**API stable et déclarée, dont l'implémentation reste à confirmer (probablement spec ou partielle)**.
Ce n'est **pas** une bibliothèque éprouvée en production : c'est le contrat d'interface que le
pipeline d'assets de Noge promet de tenir.

- **Namespace** : `nkentseu` (tous les types ci-dessous). Les types ECS sont sous `nkentseu::ecs::`
  (`NkSkeleton`, `NkAnimationClip`, `NkWorld`, `NkEntityId`).
- **Headers réels** : `Noge/IO/NkFBXImporter.h`, `Noge/IO/NkGLTFIO.h`, `Noge/IO/NkOBJIO.h`,
  `Noge/IO/NkSVGIO.h`.
- **Note math** : `NkGLTFIO.h` fait `using namespace math;` au niveau du namespace `nkentseu` ;
  les types `NkVec*`, `NkMat4f`, `NkQuatf` y apparaissent non qualifiés, mais vivent dans
  `nkentseu::math`.

---

## Lire une scène complète : FBX et glTF

FBX et glTF partagent une même idée : un fichier n'est pas *un* objet, c'est **une scène entière** —
plusieurs maillages, leurs matériaux, un squelette, des animations, une hiérarchie de nœuds. Les
importeurs correspondants renvoient donc un objet **scène** (`NkFBXScene`, `NkGLTFScene`) qui agrège
tout cela dans des `NkVector`. On instancie un importeur, on appelle `Import`, on vérifie la
**validité** de la scène retournée, et on se sert ensuite de ce qu'elle contient.

```cpp
NkFBXImporter imp;
NkFBXScene s = imp.Import("personnage.fbx");
if (s.valid) {
    // s.meshes, s.skeletons, s.animations, s.materials sont remplis
} else {
    // diagnostic : imp.GetLastError()  (et s.lastError, distinct)
}
```

Deux subtilités à retenir tout de suite. D'abord, l'état d'erreur existe **en double** : l'importeur
porte son propre `GetLastError()`, et la scène a son champ `lastError` à elle — ce ne sont pas le
même. Ensuite, FBX et glTF ne sont **pas** au même niveau de richesse. FBX est volontairement
minimal ici : sa scène est un simple sac de données (maillages, squelettes, animations, matériaux),
sans aucune méthode pour l'instancier dans le monde. glTF, le **format prioritaire**, va beaucoup
plus loin : sa scène sait `SpawnIntoWorld` (peupler directement l'ECS) et `MergeAllMeshes` (fusionner
tout en un seul maillage), et elle décrit finement matériaux PBR, morph targets, skinning et courbes
d'animation.

> **En résumé.** FBX et glTF renvoient une **scène multi-objets** par valeur, qu'on valide avant
> usage. FBX = sac de données legacy, sans instanciation ; **glTF = format prioritaire**, complet
> (PBR, skinning, morph, animations) et capable de se déverser tout seul dans l'ECS via
> `SpawnIntoWorld`. Toujours vérifier `valid`/`IsValid()` puis `GetLastError()`.

---

## Lire/écrire un seul maillage : OBJ

OBJ ne pense pas en scènes : il pense en **un maillage**. Le format est le pivot universel, lisible
par tout — c'est le format « ça marche partout » pour échanger une géométrie sans se poser de
questions. En conséquence, `NkOBJIO::Import` renvoie directement **un `NkEditableMesh`** (fusionné),
pas une scène, et l'export prend un maillage pour produire un `.obj` (plus un `.mtl` pour les
matériaux, optionnel).

L'autre particularité d'OBJ : **l'API est entièrement statique**. Pas d'objet à instancier — on
appelle `NkOBJIO::Import(...)` / `NkOBJIO::Export(...)` directement sur la classe. C'est commode,
mais cela a un revers à connaître : le `GetLastError()` est **statique** lui aussi, donc l'état
d'erreur est **global et partagé**. Deux threads qui importent en même temps se marchent dessus sur
le dernier message d'erreur — ce n'est **pas** réentrant.

```cpp
NkEditableMesh mesh = NkOBJIO::Import("model.obj");
// ... vérifier la validité selon NkEditableMesh ...
bool ok = NkOBJIO::Export(mesh, "out.obj");
if (!ok) { /* NkOBJIO::GetLastError() — attention : global */ }
```

> **En résumé.** OBJ = **un seul maillage**, pas une scène : `Import` renvoie un `NkEditableMesh`,
> `Export` écrit `.obj` (+ `.mtl`). API **100 % statique** avec `GetLastError()` statique → état
> d'erreur **global, non réentrant** : à manier prudemment en multithread.

---

## Le design vectoriel 2D : SVG

SVG sort du monde 3D : c'est du **dessin vectoriel** (chemins, formes, dégradés, texte), le format
d'Illustrator et d'Inkscape. Il alimente la branche *Design* de Noge, et travaille donc sur un
`NkVectorDocument` (un document vectoriel, avec ses artboards et ses chemins `NkVectorPath`), pas sur
un maillage 3D.

Ici, encore une convention différente des trois autres. Là où glTF/OBJ **renvoient** leur résultat,
`NkSVGIO::Import` **remplit un `NkVectorDocument&` passé par référence** (out-param) et renvoie un
simple `bool` de succès. C'est un troisième idiome dans une même famille — il faut le savoir pour ne
pas chercher une valeur de retour qui n'existe pas. Comme OBJ, l'API SVG est **entièrement statique**
(même piège de l'erreur globale), et elle offre en bonus deux utilitaires de bas niveau : `PathToSVG`
(sérialiser un chemin en attribut `d`) et `SVGToPath` (parser un attribut `d`).

```cpp
NkVectorDocument doc;
if (NkSVGIO::Import("logo.svg", doc)) {
    // doc est rempli : groupes, formes, chemins…
}
bool ok = NkSVGIO::Export(doc, "out.svg", /*artboardIdx*/0);
```

> **En résumé.** SVG est le **2D vectoriel** (sur `NkVectorDocument`, pas un maillage). Idiome
> propre : `Import` **remplit un out-param** `NkVectorDocument&` et renvoie `bool` (≠ glTF/OBJ qui
> retournent par valeur). API statique (erreur globale) + utilitaires `PathToSVG`/`SVGToPath`.

---

## Trois conventions à ne pas confondre

Avant la référence, le récapitulatif des **différences de forme** entre les quatre modules, parce
que c'est la source d'erreur numéro un :

- **Retour** : FBX/glTF → une **scène** par valeur ; OBJ → **un maillage** par valeur ; SVG →
  **out-param** `NkVectorDocument&` + `bool`.
- **Instance** : `NkFBXImporter`/`NkGLTFImporter`/`NkGLTFExporter` = objets à **instancier** (erreur
  par instance) ; `NkOBJIO`/`NkSVGIO` = classes **100 % statiques** (erreur **globale**, non
  réentrante).
- **`scaleFactor` par défaut** : `0.01f` pour FBX (cm → m), mais `1.f` pour glTF/OBJ/SVG.

> **En résumé.** Même famille, trois patrons d'appel : scène-par-valeur (FBX/glTF), maillage-par-
> valeur (OBJ), out-param + bool (SVG) ; deux modèles d'erreur (par instance vs global statique) ;
> un défaut d'échelle qui change pour FBX seul.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence
complète » qui suit. Tous les retours porteurs d'info sont `[[nodiscard]]` ; toutes les méthodes IO
sont `noexcept` (pas d'exceptions — on vérifie `bool`/`valid`/`GetLastError`).

### FBX — `NkFBXImporter.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Options | `NkFBXImportOptions` | Quoi importer (`importMeshes/Skeleton/Animation/Materials/Lights/Cameras`), `scaleFactor=0.01f` (cm→m), `flipY`, `flipWinding`, `bakeGeomTransforms`. |
| Scène | `NkFBXScene` | `valid`, `meshes`, `skeletons`, `animations`, `materials`, `lastError`. |
| Scène (imbriqué) | `NkFBXScene::MaterialData` | `name`, `diffuseTex/normalTex/roughnessTex`, `diffuseColor`, `roughness`, `metallic`. |
| Importeur | `NkFBXImporter::Import(path, opts={})` | Lit un FBX → `NkFBXScene` (par valeur). |
| Importeur | `NkFBXImporter::GetLastError()` | Dernière erreur (par instance). |

### glTF — `NkGLTFIO.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Matériau | `NkGLTFMaterialData` | PBR Metallic-Roughness : `baseColorFactor/Texture`, `metallic/roughnessFactor`, `metallicRoughnessTexture`, `normalTexture/Scale`, `occlusionTexture/Strength`, `emissiveFactor/Texture`, `alphaMode`, `alphaCutoff`, `doubleSided`. |
| Matériau (enum) | `NkGLTFMaterialData::AlphaMode { Opaque, Mask, Blend }` | Mode de transparence (`:uint8`). |
| Maillage | `NkGLTFMeshData` | `name`, `mesh` (CPU), `materialIndex`, `morphTargets`, `skin`, `hasSkinning`. |
| Maillage (imbriqué) | `NkGLTFMeshData::MorphTarget` | `name`, `positionDeltas`, `normalDeltas`. |
| Maillage (imbriqué) | `NkGLTFMeshData::SkinData` | `boneIndices` (4/vertex), `boneWeights` (4/vertex). |
| Nœud | `NkGLTFNodeData` | `name`, `localTransform`, `translation/rotation/scale`, `meshIndex/cameraIndex/skinIndex/parentIndex` (−1 = aucun), `children`. |
| Animation | `NkGLTFAnimationData` | `name`, `duration`, `channels`. |
| Animation (imbriqué) | `NkGLTFAnimationData::Channel` | `targetNode`, `path`, `times`, `values`, `interp`. |
| Animation (enum) | `Channel::Path { Translation, Rotation, Scale, MorphWeights }` | Cible animée (`:uint8`). |
| Animation (enum) | `Channel::Interpolation { Linear, Step, CubicSpline }` | Type d'interpolation (`:uint8`). |
| Scène | `NkGLTFScene` | `name`, `valid`, `meshes`, `materials`, `nodes`, `animations`, `skeletons`, `rootNodes`. |
| Scène | `NkGLTFScene::IsValid()` | `valid` ? |
| Scène | `NkGLTFScene::SpawnIntoWorld(world, out=nullptr)` | Instancie toute la scène dans l'ECS ; `out` = entités créées. |
| Scène | `NkGLTFScene::MergeAllMeshes()` | Fusionne tous les maillages → `NkEditableMesh`. |
| Options import | `NkGLTFImportOptions` | `importMeshes/Materials/Skeletons/Animations/Cameras/Lights/BlendShapes`, `scaleFactor=1.f`, `flipY`, `flipWinding`, `textureBasePath`. |
| Importeur | `NkGLTFImporter::Import(path, opts={})` | `.gltf` ou `.glb` → scène. |
| Importeur | `NkGLTFImporter::ImportFromMemory(data, size, opts={})` | **GLB uniquement** → scène. |
| Importeur | `NkGLTFImporter::GetLastError()` | Dernière erreur (par instance). |
| Options export | `NkGLTFExportOptions` | `format`, `embedTextures`, `exportNormals/UVs/Colors/Tangents`, `exportSkinning/Animation`, `scaleFactor`. |
| Options export (enum) | `NkGLTFExportOptions::Format { GLTF, GLB }` | Format de sortie (`:uint8`). |
| Exporteur | `NkGLTFExporter::SetOptions(opts)` | Fixe les options (≠ `SetFormat` de la doc obsolète). |
| Exporteur | `NkGLTFExporter::AddMesh(mesh, mat={}, name=nullptr)` | Ajoute un maillage + matériau. |
| Exporteur | `NkGLTFExporter::AddSkeleton(skeleton)` | Ajoute un squelette. |
| Exporteur | `NkGLTFExporter::AddAnimation(clip, skeleton)` | Ajoute un clip lié à un squelette. |
| Exporteur | `NkGLTFExporter::Export(path)` | Écrit le fichier. |
| Exporteur | `NkGLTFExporter::ExportToMemory(out)` | Écrit en mémoire (GLB). |
| Exporteur | `NkGLTFExporter::GetLastError()` | Dernière erreur (par instance). |

### OBJ — `NkOBJIO.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Options import | `NkOBJImportOptions` | `triangulate`, `scaleFactor=1.f`, `flipY`, `generateNormals`. |
| Options export | `NkOBJExportOptions` | `exportNormals`, `exportUVs`, `exportMTL`, `precision=6.f` (**`float32`**). |
| IO (statique) | `NkOBJIO::Import(path, opts={})` | → **un** `NkEditableMesh` (fusionné). |
| IO (statique) | `NkOBJIO::Export(mesh, path, opts={})` | Écrit `.obj` (+ `.mtl`). |
| IO (statique) | `NkOBJIO::GetLastError()` | Erreur **globale** (statique, non réentrante). |

### SVG — `NkSVGIO.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Options import | `NkSVGImportOptions` | `importGroups/Defs/Text/ClipPaths`, `flattenGroups`, `scaleFactor=1.f`, `preserveViewBox`. |
| Options export | `NkSVGExportOptions` | `prettyPrint`, `minify`, `embedFonts`, `useAbsCoords`, `optimizePaths`, `precision=2.f` (**`float32`**), `exportMetadata`. |
| IO (statique) | `NkSVGIO::Import(path, doc&, opts={})` | Remplit `NkVectorDocument&` (out-param) → `bool`. |
| IO (statique) | `NkSVGIO::ImportFromString(svgText, doc&, opts={})` | Idem depuis une chaîne. |
| IO (statique) | `NkSVGIO::Export(doc, path, artboardIdx=0, opts={})` | Écrit un artboard → `bool`. |
| IO (statique) | `NkSVGIO::PathToSVG(path, precision=2)` | Chemin → attribut `d` (**`precision` est `uint32`**). |
| IO (statique) | `NkSVGIO::SVGToPath(d)` | Parse l'attribut `d` → `NkVectorPath`. |
| IO (statique) | `NkSVGIO::GetLastError()` | Erreur **globale** (statique). |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines —
gameplay/IA, rendu, animation, physique, audio, UI/éditeur, IO, scène. **Rappel transversal** :
ces quatre headers sont des **specs sans corps** (hors getters triviaux) ; tout ce qui suit décrit
le **contrat d'API**, pas un comportement runtime garanti.

### FBX — `NkFBXImporter`, `NkFBXScene`, `NkFBXImportOptions`

**Pourquoi FBX.** C'est le format **historique** d'Autodesk (Maya, 3ds Max) et le porteur de
beaucoup de contenus existants — animations de personnages, rigs, bibliothèques d'assets. On l'utilise
surtout pour **récupérer du legacy** ; pour le pipeline neuf, glTF est préférable.

**`NkFBXImportOptions` — décider ce qu'on lit.** Les booléens `importMeshes`, `importSkeleton`,
`importAnimation`, `importMaterials` sont à `true` par défaut ; `importLights` et `importCameras`
sont à `false` (on veut rarement la lumière/caméra de l'auteur dans le moteur). Le champ qui compte
le plus est `scaleFactor = 0.01f` : FBX exporte traditionnellement en **centimètres**, et le moteur
travaille en **mètres**, d'où ce facteur 1/100 par défaut — c'est la cause classique d'un modèle qui
arrive 100× trop gros si on le met à 1. `flipY` et `flipWinding` corrigent les conventions d'axe et
d'orientation des faces (selon le DCC source), et `bakeGeomTransforms` (à `true`) aplatit les
transformations géométriques dans les sommets.

- **Scène / éditeur** — importer une bibliothèque d'assets FBX dans l'éditeur pour les ranger.
- **Animation** — récupérer des clips d'un rig Maya existant (`importAnimation`).
- **Rendu** — `importMaterials` ramène les textures et couleurs de base via `MaterialData`.

**`NkFBXScene` — le sac de données.** Après import, `valid` dit si la lecture a réussi ; `meshes`,
`skeletons`, `animations` et `materials` sont des `NkVector` qu'on parcourt. La structure imbriquée
`NkFBXScene::MaterialData` décrit un matériau : `name`, chemins `diffuseTex`/`normalTex`/
`roughnessTex`, une `diffuseColor` (défaut gris clair `{0.8,0.8,0.8,1}`), `roughness=0.5` et
`metallic=0`. Notez bien : contrairement à glTF, **FBX n'a pas de `SpawnIntoWorld` ni de
`MergeAllMeshes`** — la scène ne sait pas s'instancier ; à vous de transférer ses maillages dans
votre monde. Et l'erreur se lit sur deux canaux distincts : `NkFBXScene::lastError` (côté scène) et
`NkFBXImporter::GetLastError()` (côté importeur).

**`NkFBXImporter` — l'objet.** Une seule méthode utile : `Import(path, opts={})`, qui rend une scène
**par valeur**. `GetLastError()` (inline, renvoie `mLastError`) donne le diagnostic. Pas de
constructeur/destructeur déclarés explicitement. L'idiome est : instancier, importer, tester
`scene.valid`, sinon lire l'erreur.

### glTF — le format prioritaire (`NkGLTFIO.h`)

**Pourquoi glTF.** C'est le **standard moderne** (Khronos), pensé pour la transmission temps réel :
PBR Metallic-Roughness natif, skinning, morph targets, animations échantillonnées, et deux variantes
de conteneur — `.gltf` (JSON + ressources externes) et `.glb` (un seul binaire auto-suffisant).
C'est le format que le pipeline de Noge **préfère**. Avertissement d'intégration : `NkGLTFIO.h`
dépend de `NkGameObjectFactory.h` et `NkECSDefines.h`, dont les chemins ECS sont **signalés
obsolètes** dans la mémoire projet — la compilation standalone peut casser de ce fait.

**`NkGLTFMaterialData` — le matériau PBR.** C'est la description complète d'un matériau Metallic-
Roughness : `baseColorFactor` (teinte, défaut blanc) + `baseColorTexture` (albédo), `metallicFactor`
(0 par défaut → non métallique) et `roughnessFactor` (1 → mat) + leur texture combinée
`metallicRoughnessTexture`, la `normalTexture` avec son `normalScale`, l'`occlusionTexture` avec son
`occlusionStrength`, l'`emissiveFactor`/`emissiveTexture` pour l'auto-émission. La transparence se
règle par l'enum **`NkGLTFMaterialData::AlphaMode { Opaque, Mask, Blend }`** (sous-jacent `uint8`) :
`Opaque` (défaut) ignore l'alpha, `Mask` découpe au seuil `alphaCutoff=0.5` (feuillages, grilles),
`Blend` mélange (verre, fumée). `doubleSided` désactive le back-face culling.

- **Rendu** — alimente directement un shader PBR : albédo, métal/rugosité, normal mapping, AO,
  émission. C'est la donnée pivot du rendu réaliste.
- **Éditeur** — l'inspecteur de matériau mappe quasi un-pour-un sur ces champs.

**`NkGLTFMeshData` — la géométrie + ses extras.** Outre `name`, le `mesh` (un `NkEditableMesh`,
données CPU) et `materialIndex`, deux structures imbriquées portent l'avancé. `MorphTarget` (`name`,
`positionDeltas`, `normalDeltas`) décrit une **forme cible** — la base des blend shapes faciaux et
des déformations (sourire, clignement). `SkinData` (`boneIndices` et `boneWeights`, **4 par
sommet**) porte le **skinning** : à quels os chaque sommet est rattaché et avec quel poids ;
`hasSkinning` dit si le maillage est rigué.

- **Animation** — `MorphTarget` pour le facial/blend shapes, `SkinData` pour l'animation
  squelettique des personnages.
- **Rendu** — le skinning se fait au vertex shader à partir de ces indices/poids.

**`NkGLTFNodeData` — la hiérarchie.** Chaque nœud porte un nom, une `localTransform` (matrice,
identité par défaut) **et** sa décomposition TRS (`translation`, `rotation` quaternion identité,
`scale` à `{1,1,1}`) — utile selon qu'on raisonne en matrice ou en composantes. Les liens vers les
ressources sont des **index** : `meshIndex`, `cameraIndex`, `skinIndex`, `parentIndex`, tous à **−1**
quand il n'y a rien, et `children` liste les enfants. C'est l'**arbre de scène** : on le parcourt
depuis les racines pour instancier la hiérarchie d'entités.

- **Scène** — reconstruire le graphe parent/enfant dans l'ECS (transform parenting).
- **Caméra** — un nœud avec `cameraIndex ≥ 0` place une caméra importée.

**`NkGLTFAnimationData` — les courbes.** Une animation a un `name`, une `duration`, et une liste de
`channels`. Chaque **`Channel`** cible un nœud (`targetNode`) et une propriété via l'enum
**`Channel::Path { Translation, Rotation, Scale, MorphWeights }`** (membre `path`) : on anime donc la
position, la rotation, l'échelle d'un nœud, ou les poids de morph targets. Les `times` sont les
abscisses (clés temporelles), les `values` les valeurs — **Vec3 pour les TRS, Vec4 pour les
quaternions** (rotation). Le membre `interp`, de type **`Channel::Interpolation { Linear, Step,
CubicSpline }`**, dit comment interpoler entre deux clés : linéaire, en escalier (pas
d'interpolation), ou spline cubique (courbes lisses avec tangentes). Toutes ces enums sont `uint8`
et **doublement imbriquées** (à qualifier `NkGLTFAnimationData::Channel::Path::Rotation`).

- **Animation** — c'est le cœur de la lecture d'animation : un système échantillonne chaque canal à
  `t` selon `interp` et applique aux nœuds/morphs.
- **Gameplay** — déclencher/blender des clips importés sur les personnages.

**`NkGLTFScene` — la scène déversable.** Tout est là : `meshes`, `materials`, `nodes`, `animations`,
`skeletons`, plus `rootNodes` (les index des racines de la hiérarchie) et `valid`. Deux méthodes la
distinguent de FBX. `IsValid()` (inline) reflète `valid`. **`SpawnIntoWorld(world, out=nullptr)`**
instancie **toute la scène** dans un `ecs::NkWorld` — c'est le bouton « charger ce modèle dans le
jeu » : il crée entités, transforms, maillages, matériaux d'un coup, et remplit le `out` optionnel
avec les `NkEntityId` créés (pour les retrouver/manipuler ensuite). Attention : la doc Doxygen
mentionne un paramètre `factory`, mais la **vraie signature n'a que `world` + `out`** — pas de
factory. Enfin, **`MergeAllMeshes()`** fusionne tous les maillages en un seul `NkEditableMesh` —
pratique quand on veut juste la géométrie agrégée (collision statique, prévisualisation, export
re-fusionné) sans la hiérarchie.

- **Scène / gameplay** — `SpawnIntoWorld` pour charger un personnage ou un décor complet en une ligne.
- **Physique** — `MergeAllMeshes` pour bâtir un mesh de collision statique à partir d'un décor.

**`NkGLTFImportOptions` — quoi lire.** Tous les `import*` (Meshes, Materials, Skeletons, Animations,
Cameras, Lights, BlendShapes) sont à `true` ; `scaleFactor=1.f` (glTF est déjà en mètres, d'où la
différence avec FBX), `flipY`/`flipWinding` pour les conventions, et `textureBasePath` indique le
**dossier des textures externes** (indispensable pour un `.gltf` qui référence ses images à côté).

**`NkGLTFImporter` — l'objet importeur.** Ctor/dtor `= default`. `Import(path, opts={})` lit un
`.gltf` **ou** un `.glb`. `ImportFromMemory(data, size, opts={})` lit depuis un buffer mais **GLB
uniquement** (per doc) — typiquement un asset embarqué ou téléchargé. `GetLastError()` (inline) pour
le diagnostic. Les méthodes privées (`ParseGLB`/`ParseGLTF`, `LoadMeshes/Materials/Nodes/Animations/
Skeletons`) ne font pas partie de l'API publique.

- **IO** — `ImportFromMemory` pour streamer un GLB depuis le réseau ou un pack d'assets.

**`NkGLTFExportOptions` — quoi écrire.** L'enum **`Format { GLTF, GLB }`** choisit le conteneur
(`format=GLB` par défaut — le binaire auto-suffisant). `embedTextures` (true) inline les images,
`exportNormals/UVs/Tangents` (true) et `exportColors` (false) sélectionnent les attributs de sommet,
`exportSkinning`/`exportAnimation` (true) embarquent rig et clips, `scaleFactor=1.f`.

**`NkGLTFExporter` — l'objet exporteur.** Ctor/dtor `= default`. On configure avec
**`SetOptions(opts)`** (inline) — et **non** `SetFormat`, malgré l'exemple obsolète de l'en-tête. On
empile le contenu : `AddMesh(mesh, mat={}, name=nullptr)` (maillage + matériau + nom),
`AddSkeleton(skeleton)`, `AddAnimation(clip, skeleton)` (un clip rattaché à son squelette). Puis on
écrit : `Export(path)` (fichier) ou `ExportToMemory(out)` (buffer GLB). `GetLastError()` (inline)
diagnostique l'échec. Là encore, **fiez-vous aux signatures, pas aux commentaires** : la doc montre
`SetFormat(...)` et `AddMesh(mesh, materialDesc)`, le code expose `SetOptions(...)` et `AddMesh(mesh,
mat, name)`.

- **Éditeur / IO** — exporter une sélection de l'éditeur vers `.glb` pour la partager.
- **Pipeline** — re-sérialiser un asset transformé (LOD généré, mesh fusionné) en GLB.

### OBJ — `NkOBJIO`, `NkOBJImportOptions`, `NkOBJExportOptions`

**Pourquoi OBJ.** C'est le **dénominateur commun** : un format texte simple, sans squelette ni
animation, lu et écrit par absolument tout. On s'en sert pour les échanges robustes de **géométrie
brute** — un prop, un mesh de collision, un test rapide.

**`NkOBJImportOptions`.** `triangulate=false` (forcer la triangulation des faces n-gones si `true`),
`scaleFactor=1.f`, `flipY=false`, `generateNormals=true` (recalculer les normales absentes — fréquent
en OBJ où elles manquent souvent).

**`NkOBJExportOptions`.** `exportNormals`/`exportUVs` (true), `exportMTL=true` (écrire le fichier
matériau `.mtl` à côté), et `precision=6.f` — le **nombre de décimales** des coordonnées. Subtilité :
ce `precision` est typé **`float32`** alors qu'il compte des décimales (un entier eût été plus
logique) ; c'est un piège de cohérence à retenir.

**`NkOBJIO` — API statique.** Pas d'instance. `Import(path, opts={})` rend **un seul** `NkEditableMesh`
fusionné (contraste net avec FBX/glTF qui rendent une scène multi-objets). `Export(mesh, path, opts={})`
écrit le `.obj` (+ `.mtl`). `GetLastError()` est **statique** → l'état d'erreur est **global et
partagé** : non thread-safe, à isoler si plusieurs imports concurrents.

- **Physique** — importer/exporter un mesh de collision simple.
- **IO / éditeur** — échange rapide de géométrie avec un autre outil.
- **Rendu** — récupérer un prop statique sans rig.

### SVG — `NkSVGIO`, `NkSVGImportOptions`, `NkSVGExportOptions`

**Pourquoi SVG.** C'est le format du **vectoriel 2D** (formes, chemins de Bézier, dégradés, texte) —
le pont vers Illustrator/Inkscape, et l'alimentation de la branche *Design* de Noge. Il travaille sur
un `NkVectorDocument` (avec ses artboards et chemins `NkVectorPath`), pas sur de la 3D. L'en-tête
inclut `Nkentseu/Design/Vector/NkVectorDocument.h` ; le chemin logique commenté est
`Nkentseu/Design/IO/NkSVGIO.h`.

**`NkSVGImportOptions`.** `importGroups`/`importDefs`/`importText`/`importClipPaths` (true) activent
chaque famille d'éléments SVG, `flattenGroups=false` fusionne tous les groupes en **une seule couche**
si `true`, `scaleFactor=1.f`, `preserveViewBox=true` garde le cadrage d'origine.

**`NkSVGExportOptions`.** `prettyPrint=true` (XML lisible) vs `minify=false` (compact), `embedFonts`
(polices en **base64**), `useAbsCoords` (coordonnées absolues vs relatives dans les chemins),
`optimizePaths=true` (simplification), `precision=2.f` (décimales, encore typé **`float32`**),
`exportMetadata=true`.

**`NkSVGIO` — API statique.** `Import(path, doc&, opts={})` et `ImportFromString(svgText, doc&,
opts={})` **remplissent un `NkVectorDocument&`** (out-param) et renvoient `bool` — idiome distinct des
autres modules. `Export(doc, path, artboardIdx=0, opts={})` écrit un artboard donné (`0` = le
premier). Deux utilitaires bas niveau : `PathToSVG(path, precision=2)` sérialise un `NkVectorPath` en
attribut `d` — ici **`precision` est `uint32`** (incohérence à noter avec le `float32 precision` des
structs d'options) — et `SVGToPath(d)` parse un attribut `d` en `NkVectorPath`. `GetLastError()` est
statique (erreur globale, même réserve de réentrance).

Capacités **déclarées** (commentaire d'en-tête, **cibles** et non garanties) : import des formes
`rect/circle/ellipse/line/polyline/polygon/path`, transforms `translate/rotate/scale/matrix/skewX/Y`,
styles `fill/stroke/stroke-width/opacity/stroke-dasharray`, dégradés linéaires/radiaux, texte
`text/tspan` basique, groupes `g/use/defs/symbol`, clip-paths & masques ; export des commandes path
`M/L/C/Q/A/Z` avec merge/simplification.

- **UI / éditeur** — importer un logo ou des icônes vectorielles, exporter un dessin créé dans
  l'éditeur Design.
- **IO** — `SVGToPath`/`PathToSVG` comme briques de bas niveau pour bricoler des chemins.
- **Rendu 2D** — alimenter un rasteriseur vectoriel à partir d'un `NkVectorDocument` importé.

### Pièges transversaux à mémoriser

- **enums à qualifier** : `NkGLTFMaterialData::AlphaMode::{Opaque,Mask,Blend}` ;
  `NkGLTFAnimationData::Channel::Path::{Translation,Rotation,Scale,MorphWeights}` ;
  `NkGLTFAnimationData::Channel::Interpolation::{Linear,Step,CubicSpline}` ;
  `NkGLTFExportOptions::Format::{GLTF,GLB}`. Toutes `: uint8`.
- **docs d'en-tête obsolètes (glTF)** : l'exemple emploie `SetFormat(...)` et
  `SpawnIntoWorld(world, factory)` ; le code expose `SetOptions(...)` et
  `SpawnIntoWorld(world, out=nullptr)`. **Se fier aux signatures.**
- **`precision` incohérent** : `float32` dans `NkOBJExportOptions`/`NkSVGExportOptions`, mais
  `uint32` dans `NkSVGIO::PathToSVG`.
- **statut d'implémentation** : aucun corps non-trivial dans ces headers ; les marqueurs ✅/⚠️/❌ de
  `NkGLTFIO.h` énumèrent des **cibles** (`KHR_draco_mesh_compression` = « Phase 2 », Ray Tracing =
  « non prévu ❌ »), pas des garanties runtime. **API déclarée, implémentation à confirmer.**

---

### Exemple

```cpp
#include "Noge/IO/NkGLTFIO.h"
#include "Noge/IO/NkOBJIO.h"
#include "Noge/IO/NkSVGIO.h"
using namespace nkentseu;

// glTF : charger un personnage complet directement dans l'ECS.
NkGLTFImporter gimp;
NkGLTFScene scene = gimp.Import("character.glb");
if (scene.IsValid()) {
    NkVector<ecs::NkEntityId> created;
    scene.SpawnIntoWorld(world, &created);     // entités + transforms + meshes + matériaux
} else {
    /* gimp.GetLastError() */
}

// glTF : exporter une géométrie en GLB.
NkGLTFExporter gexp;
NkGLTFExportOptions opt;
opt.format = NkGLTFExportOptions::Format::GLB;   // enum à qualifier
gexp.SetOptions(opt);                            // PAS SetFormat
gexp.AddMesh(mesh, mat, "obj");
if (!gexp.Export("out.glb")) { /* gexp.GetLastError() */ }

// OBJ : un seul maillage, API statique.
NkEditableMesh prop = NkOBJIO::Import("prop.obj");
bool okObj = NkOBJIO::Export(prop, "prop_out.obj");

// SVG : Import remplit un out-param (≠ retour par valeur).
NkVectorDocument doc;
if (NkSVGIO::Import("logo.svg", doc)) {
    NkSVGIO::Export(doc, "logo_out.svg", /*artboardIdx*/0);
}
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
