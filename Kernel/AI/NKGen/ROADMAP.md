# NKGen — Roadmap

> Créer du contenu par l'IA (Phase 6). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — image 2D (Phase 6) 🟡
- ✅ **Auto-encodeur** (`gen::NkAutoencoder`, `src/NKGen/NkAutoencoder.{h,cpp}`) :
  encodeur `D→H→L` + décodeur `L→H→D→sigmoid`, au-dessus de NKNN. Entraîné en
  reconstruction (MSE + Adam).
- ✅ **Génération prouvée** (`NKGenTest`, `jenga run`) : images 8×8 (4 motifs bruités) →
  reconstruction **débruitée** (MSE 0.20 → **0.0021**), et **interpolation latente** entre
  deux motifs produit des images **inédites** et cohérentes (le latent est continu).
- ✅ **VAE** (`gen::NkVAE`, `src/NKGen/NkVAE.{h,cpp}`) : encodeur → (μ, logσ²),
  reparamétrisation `z = μ + exp(0.5·logσ²)·ε`, décodeur, `gen::KLDivergence`. **Génération
  inconditionnelle PROUVÉE** (`NKVAETest`) : **z ~ N(0,1) → Decode → images inédites**.
  Création **sans image source**. (Ops autograd : Exp/MulScalar/AddScalar.)
- ✅ **VAE sur VRAIES images (MNIST) → génère des CHIFFRES reconnaissables** (`NKMnistVAETest`,
  `NK_MNIST_DIR`) : reconstruction nette + **génération depuis bruit = chiffres plausibles**
  (`Build/mnist_generated.png`). Débloqué par **mini-batch** (le full-batch collapsait vers
  l'image moyenne → flou) + perte **BCE** (`nn::BCELoss`/`autograd::SigmoidBCE`, plus nette
  que MSE ; `NkVAE::DecodeLogits`). BCE 0.52 → **0.129**.
- ✅ **VAE convolutionnel** (`gen::NkConvVAE`, `src/NKGen/NkConvVAE.{h,cpp}`) : encodeur
  Conv2D→ReLU→MaxPool→latent, décodeur latent→Dense→reshape→**resize-conv**→sigmoid.
- ✅ **Conv accélérée GPU (im2col+matmul)** : `autograd::Conv2D` = im2col→ops::Matmul
  (auto-GPU). Correct (19/19) et **93× plus vite** en régime (`NKConvBenchTest`).
- 🟡 **VAE conv sur MNIST** (`NKMnistConvVAETest`, mini-batch+BCE) : reconstruit/génère des
  chiffres MAIS le **ConvTranspose créait des artefacts damier**. **Fix : décodeur en
  RESIZE-CONV** (`autograd::Upsample2x` nearest ×2, gradient-check 20/20, + Conv2D lisse) →
  **artefacts supprimés, VÉRIFIÉ** (traits lisses ; BCE **0.135 en 35 époques**, mieux que
  0.145 en 55 avec ConvTranspose). (Constat : MNIST simple = dense suffit ; le conv paie sur
  images riches multi-canaux, là où le GPU conv est indispensable.)
- ✅ **Diffusion (débruitage itératif) — PROTOTYPE JOUET 2D CPU livré** (`gen::NkDiffusion`,
  `src/NKGen/NkDiffusion.{h,cpp}` ; app `NKDiffusionTest`) : DDPM au sens de **Ho, Jain,
  Abbeel, « Denoising Diffusion Probabilistic Models », NeurIPS 2020, arXiv:2006.11239**
  (cité en tête de fichier, algorithme suivi au pied de la lettre — forward fermé éq. 4,
  paramétrisation **ε-prediction** §3.2, échantillonnage Algorithme 2). Forward : schedule
  de bruit **linéaire** (`gen::NkDiffusionSchedule::Linear`, forme du papier §4, constantes
  réadaptées à un T réduit pour que ᾱ_T→0). Reverse : petit **MLP** (`Dense→relu→Dense→relu
  →Dense`) prédisant le bruit, conditionné sur le pas de temps via un **embedding sinusoïdal
  fixe** (encodage positionnel Transformer, Vaswani et al. 2017) concaténé **à la main** au
  point bruité (pas d'opérateur de concat « axe features » dans NKAutograd, seulement
  `Concat0` sur l'axe batch). Dataset synthétique **2D (cercle, rayon 2 ± jitter 0.08, 300
  points)**, généré en C++, aucun fichier externe. **Mesure chiffrée AVANT/APRÈS
  entraînement** (`NKDiffusionTest`, `jenga build --target NKDiffusionTest --config Debug
  --platform Windows` puis exécution réelle) : distance moyenne au plus proche voisin
  d'entraînement **147.8 (avant, poids aléatoires) → 0.109 (après, 4000 époques)** ; rayon
  généré mean/std **149.9/100.6 (avant) → 1.971/0.169 (après)**, à comparer au rayon
  d'entraînement mesuré 2.001/0.047. Perte L2(bruit prédit, bruit réel) 0.99 → **~0.24-0.3**
  (bruitée par le tirage aléatoire de pas de temps par lot, mais tendance nette à la baisse).
  ⚠️ **Prototype jouet strict** : 2D seulement, aucun conditionnement, aucune sortie
  image/3D, aucun branchement NKImage/NKRHI (hors scope de ce jalon). Conditionnement et
  extension 3D toy : cf mises a jour ci-dessous et Jalon 2.
- ✅ Diffusion etendue a un nuage de points 3D JOUET (sphere, `NKDiffusionTest` section 3,
  `main.cpp`) : `gen::NkDiffusion` est generique sur `dataDim` (deja le cas avant cette
  extension), donc le meme code reverse/forward generalise directement a 3 dimensions sans
  modifier la classe. Dataset synthetique sphere 3D (rayon 2, jitter radial 0.08, 300
  points, direction uniforme via 3 gaussiennes normalisees, Marsaglia 1972). Mesure
  chiffree AVANT/APRES (4000 epoques) : distance moyenne au plus proche voisin 575.5 ->
  0.293 ; rayon genere mean/std 577.5/589.8 (avant) -> 2.017/0.230 (apres), a comparer au
  rayon d'entrainement mesure 2.000/0.047. 4/4 verifications chiffrees OK. AVERTISSEMENT :
  ceci est un NUAGE DE POINTS BRUT (positions 3D independantes), PAS un maillage -- aucune
  topologie/face, aucune extraction de surface. Pas de Conv3D (le MLP dense generalise tel
  quel). Pas de sortie image/voxel.
- ⬜ Marching cubes (extraction de surface, Lorensen & Cline 1987) sur un champ de densite
  -- NON traite dans cette passe : table de correspondance 256 cas trop volumineuse/sujette
  a erreur a reproduire fiablement sans reference a verifier visuellement ; juge plus
  honnete de ne pas le tenter que de livrer une implementation non verifiee (risque de
  maillage non-manifold/troue silencieux). Reste a faire.
- ⬜ Conv3D im2col (generatif 3D convolutionnel accelere) ; diffusion etendue a l'image/voxel
  dense (au-dela du nuage de points jouet ci-dessus).
- ⬜ Sortie exploitable comme **texture/asset** dans le moteur (brancher NKImage/NKRHI).
- 🎯 ✅ Générer une image — reste : l'**afficher dans Nkentseu** via le pipeline d'assets
  (le pont maillage→moteur est déjà prouvé côté 3D).

## Jalon 2 — guidage & contrôle
- ✅ **Conditionnement par classe PROUVÉ** (`gen::NkDiffusion`, `NkDiffusion.{h,cpp}` ;
  `NKDiffusionTest` section 2, `main.cpp`) : conditionnement simple par **concaténation
  manuelle d'un one-hot de classe** à l'entrée du MLP epsilon_theta, en plus de l'embedding
  temporel déjà existant (constructeur `NkDiffusion(dataDim, timeEmbedDim, hidden, seed,
  numClasses)`, `numClasses=0` = comportement inconditionnel d'origine inchangé). Pattern
  cf **Mirza & Osindero, « Conditional Generative Adversarial Nets », 2014,
  arXiv:1411.1784** (concaténation d'un label à l'entrée). ⚠️ **PAS** le *classifier-free
  guidance* de **Ho & Salimans, 2022, arXiv:2207.12598** (documenté honnêtement dans
  `NkDiffusion.h`) : pas de dropout du label à l'entraînement, pas d'échelle de guidage au
  sampling — conditionnement direct simple. Dataset 3 classes/formes 2D (cercle rayon 2,
  carré demi-côté 2, spirale d'Archimède, jitter 0.08, 300 points/classe). **Mesure
  chiffrée PAR CLASSE** (matrice 3×3 distance moyenne au plus proche voisin
  généré(classe c) vs train(classe c'), 6000 époques) : diagonale (bonne classe)
  **0.115 (cercle) / 0.116 (carré) / 0.198 (spirale)**, très inférieure aux distances
  hors-diagonale (classe demandée ≠ classe train, **0.20 à 0.81**) — **6/6 vérifications
  chiffrées OK** : pour chaque label demandé, la distance minimale de la ligne est bien
  sur la diagonale (le label pilote effectivement la forme produite) ET la distance
  intra-classe est < 0.5 (forme proche de sa classe réelle).
- ⬜ Variations contrôlées (intensité/échelle de guidage — non traité). Note : la
  **graine** (seed) de l'échantillonnage est déjà un paramètre exposé (`Sample(...,
  rngSeed, ...)`), donc des tirages différents à label fixé sont déjà possibles
  trivialement ; ce qui manque est un contrôle explicite de l'**intensité**
  (ex. guidance scale à la Ho & Salimans 2022) — non implémenté.

## Jalon 3 — 3D & animation
- 🟡 Génération de **formes 3D** (procédural guidé par apprentissage) : représentations
  maillage / voxel / champ de distance signée (SDF) / nuage de points, converties en
  **maillages** exploitables par le moteur.
  - ✅ **Premier jalon 3D atteint** (`NKGen3DTest`, `jenga run`) : auto-encodeur sur
    **voxels 6×6×6** (sphère, cube, cylindre, pyramide + bruit) → reconstruction
    débruitée (MSE 0.186 → **0.00145**) et **génération par morphing latent**
    sphère→cube (formes 3D inédites, transition continue). Même stack que la 2D.
  - ✅ **VAE 3D CONVOLUTIONNEL → bruit → forme 3D → maillage OBJ** (`gen::NkVoxelVAE`,
    `src/NKGen/NkVoxelVAE.{h,cpp}` ; op autograd **Conv3D/ConvTranspose3D**, couches
    `nn::NkConv3D`/`NkConvTranspose3D`, backward vérifiés ; app `NKVoxelGenTest`) :
    encodeur Conv3D (stride) → latent → décodeur ConvTranspose3D. Entraîné (recon MSE
    0.226 → **0.0068**), puis **z ~ N(0,1) → forme 3D inédite** (115/512 voxels) →
    `VoxelsToMesh` → **OBJ** (648 v / 324 tris). **Chaîne complète « bruit → asset 3D ».**
  - ✅ **Voxels → maillage DENSE (proxy) → OBJ** (`gen::VoxelsToMesh`/`SaveMeshObj`,
    `NkMesh.{h,cpp}` ; app `NKGenMeshTest`) : une forme **générée par l'IA** est convertie
    en triangles (culling des faces internes) et exportée en **.OBJ** valide (384 sommets /
    192 tris). ⚠️ **Maillage de PROXY / prévisualisation**, PAS une topologie de production.

### Pipeline d'asset 3D correct (à compléter) — **la génération de maillage passe par la RETOPOLOGIE**
  Un asset production ne sort JAMAIS brut du champ scalaire. Étapes :
  1. **Surface dense** : ✅ **Surface Nets** (`gen::SurfaceNets`, `NkMesh.{h,cpp}` ; app
     `NKSmoothMeshTest`) — surface **lisse, quad-dominante, sommets soudés** depuis un
     champ scalaire (métaballs 40³ : 1854 sommets vs 7408 pour le proxy, à triangles
     égaux). ⬜ variante **marching cubes** + grilles plus fines + décodeur conv 3D.
  2. 🟡 **RETOPOLOGIE** → maillage **propre** (quads, *edge-flow* suivant la forme :
     boucles autour des articulations/visage pour une déformation correcte à l'animation).
     - ✅ **Passe 1 — décimation par clustering** (`gen::DecimateClustering`, app
       `NKSmoothMeshTest`) : soudure + allègement (1854→706 sommets, **−62% triangles**),
       indices valides, OBJ `nkgen_retopo.obj`. Base low-poly.
     - ✅ **Maillage QUAD natif** (`gen::SurfaceNetsQuads`) : remesh **quad-dominant**
       uniforme (faces `f a b c d`, 1852 quads) — OBJ `nkgen_quads.obj`. Plus de soupe de
       triangles. (Export OBJ gère quads + tris.)
     - ✅ **QEM** (quadric error) — `renderer::NkMeshDecimate` (NKRenderer/Mesh, 31/07) :
       contraction d'arêtes avec condition de lien, anti-retournement, rétention des bords
       et plafond d'erreur. Posé sur `NkEditMesh` (la structure de NK3DModeler), pas sur
       `gen::NkMesh` — s'y brancher via OBJ ou indexed en attendant un pont direct.
       Preuves : plan 128→19 tris à erreur **exactement nulle** ; cube subdivisé 768→268
       fermé/manifold (batteries `decim/` du NKEditMeshHarness).
     - 🟡 **Quad field-aligned** — livré (31/07) : `renderer::NkMeshRetopo`, champ de croix
       (épinglage arêtes vives/bords + lissage 4-RoSy) et fusion **quad-dominante** guidée
       par le champ. Après décimation, `Quadify` (paires consécutives) ne trouve que 43
       quads là où le champ en fait 108 ; alignement 0,999 ; fermé/manifold préservés
       (batteries `retopo/`). **Reste ⬜** : l'extraction par grille entière façon
       *Instant Meshes* (100 % quads, contrôle des singularités) — la cible « pro ».
     - ⬜ Retopologie **apprise** (neurale) — via la stack NKNN.
  3. ⬜ **UV unwrap** + **bake** (normal/AO du dense vers le low-poly : détail préservé).
  4. ⬜ **Rig + skinning** (squelette auto) → prêt à animer.
  5. 🟡 Branchement au **pipeline d'assets / éditeur** du moteur.
     - ✅ **Rendu de prévisualisation** (`NKMeshRenderTest`) : rasteriseur logiciel
       from-scratch (caméra perspective + z-buffer + Lambert) → image PPM/PNG du maillage
       **généré** (43k px couverts) → on **voit** la forme (`Build/nkgen_render.png`).
     - ✅ **Maillage prêt-moteur** : `gen::ComputeNormals` (normales par sommet lisses) +
       export OBJ complet (`vn` + faces `v//vn`) + `gen::BuildInterleavedPN` (buffer
       pos+normal) → consommable par `NkMeshSystem::Import` / `NkMeshDesc::Simple`.
     - ✅ **Rendu GPU dans le moteur (NKRenderer v5.0) — VÉRIFIÉ À L'ÉCRAN** : `DemoNKGen`
       INTÉGRÉ au `renderdemo` (`renderdemo.exe --demo=18 -bdx11`). Génère (métaballs →
       Surface Nets) → normales → OBJ → `meshSys->Import` → `r3d->Submit`. Rendu **temps
       réel** via le pipeline **PBR 19 passes** (ombres/SSAO/bloom, caméra orbitale,
       matériau terracotta). Flag jenga `use_nkgen`. **Confirmé visuellement** (forme
       organique nette) → la forme générée par l'IA **s'affiche dans le moteur**.
     - ✅ **Log moteur capturé en fichier** (`NkFileSink` → `Build/nkgen_demo.log`,
       `logger.Flush()`) : analyse autonome de la sortie (le log confirme import 1854
       sommets + render graph 19 passes). Un des outils moteur (capture/logger) exploité.
     - ✅ **Screenshot programmatique** (`DemoNKGen` : `SetFinalColorTarget` → offscreen →
       `Capture` → PNG, une fois à la frame 63). A nécessité de **corriger le moteur** :
       `NkOffscreenTarget::ReadbackPixels` ne copiait jamais la texture couleur vers son
       staging (`CopyTextureToBuffer` + barrières `SHADER_READ↔TRANSFER_SRC` ajoutés).
       **Vérifié sur Vulkan** (`-bvk`) : `Build/nkgen_engine.png` = rendu PBR complet du
       maillage IA (ombres, grille). ⚠️ DX11 (`CopyTextureToBuffer` = stub vide) et OpenGL
       (`MapBuffer` sans bit persistant) : readback à compléter par backend.
     - ⬜ Nourrir le mesh via `NkMeshDesc` (buffers en mémoire, sans OBJ temporaire) +
       matériau PBR dédié.
- ⬜ **Catégories d'assets 3D** (objectif vision) — un générateur conditionné par type :
  - 🌿 **Végétal** : arbres, plantes, herbes (croissance procédurale + apprentissage).
  - 🐾 **Animal / créature** : morphologies quadrupèdes/volants/aquatiques.
  - 🧍 **Humanoïde** : corps + variations (morphologie, visage) pour peupler les mondes.
  - 🌍 **Monde / terrain** : reliefs, biomes, structures — mondes entiers générés.
- ⬜ **Rig + squelette** auto (pour animer les maillages générés).
- ⬜ Génération de **mouvements / animations** (marche, course, gestes) — voir aussi
  [NKEmbodied](../NKEmbodied/README.md) pour les politiques de mouvement apprises.
- ⬜ Intégration au **pipeline d'assets** du moteur (maillages, matériaux, anims) et à
  l'éditeur (prévisualisation, réglages).

## Jalon 4 — création dirigée par IA (vision « acteur / film / jeu »)
- ⬜ **Consigne → asset 3D** : décrire (texte/paramètres) un être ou un décor et le
  générer, prêt à poser dans une scène.
- ⬜ **Peuplement de mondes** : générer faune + flore + humanoïdes cohérents pour un
  biome donné, alimenter [NKCivilization](../NKCivilization/README.md).
- ⬜ **Acteurs génératifs** : un personnage reçoit un **rôle** et le joue (apparence +
  animation + comportement), pour l'**animation 3D pilotée par IA** (films, jeux) —
  combine NKGen (apparence/anim) + [NKAgent](../NKAgent/README.md)/[NKEmbodied](../NKEmbodied/README.md) (comportement).

## Plus tard
- ⬜ Modèles plus gros **hébergés** (inférence, pas entraînement frontière) ; passage à
  l'échelle sur serveurs GPU pour la qualité des assets.
- ⬜ Génération d'apparences pour [NKCivilization](../NKCivilization/README.md).
- ⬜ Assistance design dans l'éditeur (texte → asset).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
