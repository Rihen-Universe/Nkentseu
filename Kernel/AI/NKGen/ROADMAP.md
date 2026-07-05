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
- ⬜ Diffusion (débruitage itératif) ; Conv3D im2col (génératif 3D convolutionnel accéléré).
- ⬜ Sortie exploitable comme **texture/asset** dans le moteur (brancher NKImage/NKRHI).
- 🎯 ✅ Générer une image — reste : l'**afficher dans Nkentseu** via le pipeline d'assets
  (le pont maillage→moteur est déjà prouvé côté 3D).

## Jalon 2 — guidage & contrôle
- ⬜ Conditionnement (générer selon une consigne / une classe).
- ⬜ Variations contrôlées (graine, intensité).

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
     - ⬜ **QEM** (quadric error, préserve mieux la forme) sur le maillage triangulé.
     - ⬜ **Quad field-aligned** (champ de croix, façon *Instant Meshes* — edge-flow suivant
       les features : la cible qualité « pro »).
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
