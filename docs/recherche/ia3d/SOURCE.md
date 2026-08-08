# SOURCE — Survey and Evaluation of Neural 3D Shape Classification Approaches

- **Article** : Martin Mirbauer, Miroslav Krabec, Jaroslav Křivánek, Elena Šikudová,
  « Survey and Evaluation of Neural 3D Shape Classification Approaches »
  (version acceptée + matériel supplémentaire, IEEE TPAMI — méthodes publiées avant octobre 2019,
  état de l'art étendu jusqu'à fin 2020).
- **Page du projet des auteurs** : https://cgg.mff.cuni.cz/~martinm/papers/2021-survey-eval
- **Fichiers locaux lus** (2026-08-03) :
  `C:\Users\Rihen\Documents\revu\Survey-and-Evaluation-of-Neural-3D-Shape-Classification-Approaches-accepted-version.pdf` (24 p.)
  et `...-supplementary.pdf`.
- **Objet pour Nkentseu** : carte du domaine des représentations 3D neuronales, en préparation du
  chantier « modélisation par IA » de NK3DModeler (cible finale : image-to-3D et text-to-3D).

---

## 1. Ce que couvre le survey

Le survey recense **76 réseaux de neurones de CLASSIFICATION de formes 3D** (entrée : une forme 3D,
sortie : une étiquette de catégorie) et les organise par **représentation d'entrée** — c'est cette
taxonomie qui nous intéresse :

1. **Grille volumétrique (voxels)** — grille d'occupation 3D + convolutions 3D (VoxNet, 3D ShapeNets,
   Voxception-ResNet). Simple et régulière, mais la mémoire croît en cube de la résolution : en
   pratique 32³. Les **octrees** (OctNet, O-CNN, Adaptive O-CNN) lèvent partiellement la limite
   (jusqu'à 256³) en n'affinant que près de la surface.
2. **Multi-vues (images 2D)** — la forme est rendue depuis plusieurs points de vue (12 à 80), chaque
   image passe dans un CNN 2D pré-entraîné (ImageNet), puis les vues sont agrégées (max-pooling
   MVCNN, regroupement appris GVCNN, séquence RNN SeqViews2SeqLabels, pose non supervisée
   RotationNet). Variantes : profondeur, panoramas cylindriques, projections sphériques, geometry images.
3. **Nuage de points** — ensemble non ordonné de coordonnées. Familles : opérations symétriques
   (PointNet, Deep Sets), extraction hiérarchique (PointNet++, SO-Net, Kd-networks), convolution sur
   graphe de voisinage (DGCNN), convolutions sur points discrètes ou continues (PointCNN, KPConv,
   PointConv, SpiderCNN), attention (Point2Sequence, Set Transformer).
4. **Surface / maillage** — géométrie « deep learning » sur variétés (Geodesic CNN, MoNet), graphes
   (GCN), ou nativement sur le maillage (MeshNet par faces, MeshCNN par arêtes). Seule famille qui
   conserve la connectivité.
5. **Hybrides** — ensembling de classifieurs ou fusion de descripteurs (FusionNet, PVNet
   points + vues avec fusion par attention).

**Évaluation propre des auteurs** : 11 réseaux ré-entraînés sur ModelNet40 (12 311 maillages,
40 catégories) et ShapeNetCore v2 (~50 800 modèles, 55 catégories), avec un pipeline dockerisé de
conversion maillage → voxels / images / points (OpenVDB pour la voxelisation, PBRT et rendus
shaded/depth pour les vues, échantillonnage uniform / Lloyd / Poisson / Sobol pour les points).

**Résultats clés** : (a) les approches **multi-vues dominent en précision** (~91,4 % en moyenne sur
les meilleurs epochs, max ~93 %), devant voxels (~88,8 %) et nuages de points (~88,2 %) ; (b) les
accuracies **répliquées sont souvent inférieures** aux valeurs publiées (~1 pp de moins en moyenne) ;
(c) la **méthode de conversion des données change le résultat** (échantillonnage Poisson vs uniforme :
jusqu'à +0,7 pp) — le prétraitement fait partie du modèle ; (d) coût très variable : de 0,6 min/epoch
(seq2seq) à 407 min/epoch (VRN), modèles de 2,5 Mo (octree) à 550 Mo (VGG multi-vues) ; les octrees
offrent le meilleur rapport précision/coût côté volumétrique ; (e) un dataset plus grand profite
surtout aux réseaux multi-vues (+1,7 pp), les petits modèles (octree-adaptive, kdnet) saturent ;
(f) l'alignement rotationnel des modèles aide surtout les réseaux à nuages de points (+2 pp).

## 2. Ce que ça apporte au chantier IA de NK3DModeler

**Ce que ça apporte :**

- **La grille de lecture des représentations 3D**, qui est exactement celle des méthodes de
  génération : les générateurs produisent eux aussi des voxels, des points, des maillages, des vues —
  ou des champs implicites. Comprendre les forces/faiblesses recensées ici (mémoire cubique des
  voxels, absence de connectivité des points, irrégularité des maillages, perte d'information par
  occlusion des vues) permet de juger les sorties des futurs modèles génératifs et de choisir la
  représentation d'échange avec le moteur (NK3DModeler vit dans le monde du maillage : toute chaîne
  IA finira par une conversion vers maillage).
- **Une culture du pipeline de conversion** directement réutilisable : voxelisation (OpenVDB, octrees),
  rendu multi-vues, échantillonnage de surface (uniform/Lloyd/Poisson/Sobol) — briques dont un
  pipeline image-to-3D aura besoin en amont (datasets) comme en aval (post-traitement). Et la leçon
  expérimentale : **le prétraitement change les résultats**, il faut le versionner comme du code.
- **Les datasets de référence** (ModelNet40, ShapeNetCore) et leurs pièges (doublons, split officiel
  non uniforme, échelles hétérogènes → normalisation en sphère/cube unité) : point de départ pour
  toute évaluation ou tout entraînement futur.
- **Des architectures d'encodeurs éprouvées** (PointNet/PointNet++, DGCNN, encodeurs multi-vues,
  O-CNN) : dans les systèmes génératifs, ces encodeurs servent de squelette aux auto-encodeurs, aux
  fonctions de perte perceptuelles et aux évaluateurs. Le survey mentionne d'ailleurs au passage des
  briques génératives précoces utilisées ici pour la classification : 3D-GAN, 3D-ED-GAN,
  Variational Shape Learner, FoldingNet (décodeur par pliage de grille), AtlasNet-like — de bons
  ancêtres à connaître.
- **Une méthodologie d'évaluation honnête** (réplication, splits train/val/test, variance autour du
  meilleur epoch, coût mémoire/temps mesuré) à imiter quand on comparera des modèles génératifs.

**Ce que ça N'apporte PAS — à dire franchement :**

- **Ce survey porte sur la CLASSIFICATION, pas la GÉNÉRATION.** Il répond à « quelle catégorie ? »,
  jamais à « produis-moi une forme ». Aucune recette d'image-to-3D ou de text-to-3D n'y figure.
- **Les représentations implicites neuronales sont absentes** (occupancy networks, SDF appris,
  champs de radiance) : le survey s'arrête aux méthodes publiées avant octobre 2019 et ne couvre pas
  cette famille, devenue centrale en génération.
- Rien sur : le **conditionnement** (image, texte, esquisse), les **modèles de diffusion**, les
  fonctions de perte génératives, la **reconstruction multi-vues différentiable**, le rendu
  différentiable, la **texturation** ni les **matériaux** — tout ce qui fait un vrai pipeline
  image-to-3D reste à sourcer.
- Les chiffres de précision (ModelNet40 ~85-96 %) sont des scores de classification : ils ne
  prédisent en rien la qualité géométrique d'un générateur.

## 3. Ce qu'il faudra sourcer ensuite

Familles de méthodes de **génération** à documenter (un `SOURCE.md` par sujet, avec références
vérifiées au moment du sourcing — je n'en invente aucune ici) :

1. **Représentations implicites neuronales** : occupancy networks, SDF neuronaux (DeepSDF et
   descendants), champs de radiance (NeRF et variantes rapides type grilles de hachage), gaussian
   splatting. C'est le socle de la génération moderne — priorité n°1.
2. **Reconstruction single-image / few-shot supervisée** : encodeur image → décodeur 3D (voxels,
   points, maillage — la lignée Pix2Mesh est citée [4] dans le survey ; occupancy/SDF conditionnés).
3. **Modèles génératifs 3D natifs** : GAN 3D (lignée 3D-GAN déjà citée), VAE de formes, **diffusion
   sur nuages de points / latents 3D / triplans** — la famille dominante actuelle.
4. **Text-to-3D et image-to-3D par distillation 2D** : optimisation d'une représentation 3D guidée
   par un modèle de diffusion 2D (score distillation, lignée DreamFusion) et les approches
   feed-forward multi-vues plus récentes (grands modèles de reconstruction).
5. **Rendu différentiable** : rastérisation/ray-marching différentiables — l'outil transversal qui
   relie 2D et 3D dans presque toutes les méthodes ci-dessus.
6. **Extraction et remaillage** : marching cubes et équivalents différentiables, décimation,
   re-topologie — indispensable pour livrer un maillage propre à NK3DModeler.
7. **Génération de texture et de matériaux** (UV, PBR) pour que la sortie soit utilisable dans le
   moteur, pas seulement une géométrie grise.
8. **Datasets 3D à grande échelle et leurs licences** (ShapeNet exige un accord d'utilisation ;
   ModelNet est académique) : le régime juridique des DONNÉES d'entraînement devra être instruit au
   même titre que celui du code.

## 4. Régime juridique

Selon `docs/SOURCES_TIERCES.md` (règles du 2026-08-03) :

- **L'article et son supplément** : régime 1 — **libre**. Le droit d'auteur protège l'expression,
  pas les idées : la taxonomie, les méthodes décrites et les enseignements expérimentaux sont
  librement exploitables dans Nkentseu. On cite par honnêteté intellectuelle (fait ci-dessus).
- **Le code d'évaluation des auteurs** (pipeline Docker, conversions, sur la page du projet) :
  licence **non indiquée dans les PDF** → régime 5 par défaut (tous droits réservés) tant que la
  licence réelle n'a pas été vérifiée sur la page du projet. Ne rien en adapter avant vérification.
- **Les ~80 implémentations tierces référencées** (GitHub : PointNet, MVCNN, O-CNN, DGCNN, etc.) :
  chacune a **sa propre licence, à vérifier dépôt par dépôt** avant toute lecture de code destinée à
  une adaptation. Permissive (MIT/BSD/Apache/zlib) → utilisable avec en-tête conservé et mention dans
  `THIRD_PARTY_LICENSES.md`. **GPL/AGPL → exclu** : on repart de l'article correspondant, sans lire
  le code. Sans licence affichée → article seulement.
- **Les datasets** (ModelNet, ShapeNetCore) : ce sont des DONNÉES sous conditions propres
  (ShapeNet impose un accord d'utilisation non commercial à vérifier) — régime 3/5 selon le cas ;
  à instruire avant tout entraînement, et jamais à redistribuer avec le moteur.
- **Aucun code ni aucune donnée n'a été copié** dans le cadre de cette fiche : lecture d'article
  uniquement.
