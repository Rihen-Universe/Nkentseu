# MISSION IA — Vérifier et corriger l'éclairage et les ombres de NK3DModeler

> Document de passation destiné à **une IA quelconque** (pas nécessairement Claude).
> Il contient : la carte complète des codes concernés, les faits déjà prouvés
> (à ne PAS refaire), les problèmes ouverts, les pièges qui coûtent des heures,
> la boucle de vérification autonome, et le prompt prêt à copier en fin de document.
> Rédigé le 2026-08-09. Dépôt : `D:\Projets\2026\Nkentseu\Nkentseu`.

---

## 1. LA CARTE DES CODES, CLASSÉE

### 1.1 Shaders (`Resources/NKRenderer/Shaders/`) — là où tout se voit

| Fichier | Rôle |
|---|---|
| `PBR/NkSL/pbr.frag.nksl` | **LE CŒUR.** Bloc `LightsUBO` (l. ~62), boucle des 32 lumières (l. ~310) : types 0=directionnelle 1=ponctuelle 2=spot 3=surfacique, atténuation héritée/physique, cookies, GGX/diffus, diffusion sous surface (dans la boucle), appel d'ombre `SampleLightShadowEx`. Après la boucle : ambiance/IBL, vernis (clearcoat), GI voxel, brouillard, modes d'affichage |
| `PBR/NkSL/pbr.vert.nksl` | Sommets PBR (`gl_Position = uCam.viewProj * worldPos`) |
| `Include/NkShadowAtlas.glsli` | **TOUTE L'OMBRE.** Biais de pente/normal (l. 35-95), `NkSampleShadowTile` : projection dans la tuile, Y-flip DX (`globalCfg.w`), remap Z GL (`globalCfg.z`), biais du plan récepteur (Isidoro), noyaux PCF3/PCF5/Poisson choisis par `globalCfg.y`, dispatch par type de lumière (l. ~250-360) |
| `Include/NkVoxelAO.glsli` | GI voxel + AO par cônes. Grille 64×32×64, bornes **codées en dur** (−10,−5,−10)..(+10,+5,+10) |
| `PP_SSAO/NkSL/pp_ssao.{vert,frag}.nksl` | Occlusion ambiante écran v1 (Alchemy : normales reconstruites, rayon monde, bruit IGN) |
| `PP_SSAOBlur/NkSL/pp_ssaoblur.frag.nksl` | Flou gaussien 5×5 de la SSAO |
| `PP_Tonemap/NkSL/pp_tonemap.frag.nksl` | Exposition, auto-exposition, ACES, **application de la SSAO** (`hdr *= ao`, l. ~79 — multiplie SANS condition), bloom |
| `Skybox/NkSL/`, `DeferredLight/NkSL/`, `Layered*/`, `Skin/`, `CarPaint/`, `Water/`, `Glass/` | Consommateurs secondaires du même bloc de lumières (ils déclarent le bloc COURT — voir §3.4) |

### 1.2 Moteur C++ (`Kernel/Runtime/NKRenderer/src/NKRenderer/`)

| Fichier | Rôle |
|---|---|
| `Tools/Render3D/NkRender3D.cpp` | **LE PONT CPU→GPU.** `UploadUBOs` (l. ~2100) : caméra + correction clipZ01 ([-1,1]→[0,1] TOUS backends) ; bloc lumières (l. ~2500) : conversion watts en loi physique, champ `extra` (loi, largeur/hauteur surfacique). **Pipeline de la passe d'ombre** et ses biais rasterizer +64/+4/+0.02 (l. ~499-560, le POURQUOI du signe y est commenté) |
| `Tools/Render3D/NkRender3D.h` | API : `GetRenderViewProj/InvViewProj` (matrices RÉELLES du rendu), `GetSceneContext` |
| `Tools/Shadow/NkVirtualShadowMaps.cpp` | **L'ATLAS D'OMBRE** (VSM). `AllocSlotsSpot` (l. ~423 : la surfacique = spot large, plancher 45°), `AllocSlotsPoint` (l. ~477 : 6 faces + bande de garde dimensionnée sur le noyau PCF), matrices lumière + `ApplyDepthClipCorrection`, téléversement `globalCfg`/`biasParams` (l. ~705-735) |
| `Tools/Shadow/NkVirtualShadowMaps.h` | Config VSM : atlas 4096², `spotTile` 512, `pointFaceTile`, qualité, douceur, biais |
| `Tools/VoxelAO/NkVoxelAOSystem.cpp` | Grille GI voxel côté CPU/compute, injection de radiance |
| `Tools/PostProcess/NkPostProcessStack.cpp` | Passes SSAO (`DrawSSAOPass` l. ~1192 : caméra de frame, sampler NEAREST), liaison tonemap (`ExecuteRHI` : repli BLANC quand SSAO coupée), bloom dual-Kawase |
| `Core/NkRendererImpl.cpp` | Le graphe de rendu (passes Shadows/SSAO/Bloom/PostProcess l. ~830-1050), `SetPostConfig` + `FlushGraphRebuilds` (voir §3.6 — mode partagé) |
| `Core/NkRendererTypes.h` | **`NkLightDesc`** (l. ~333) : type, position, direction, couleur, intensité, portée, angles, dimensions surfacique, `attenuationMode`, température/exposition, cookie, ombres. `NkDrawCall3D` (tint/metallic/roughness/clearcoat/subsurface par drawcall) |
| `Core/NkRendererConfig.h` | Presets (`ForGame` : ssao/bloom/fxaa actifs), `NkPostConfig` (ssaoRadius EN MÈTRES, ssaoIntensity, bloom*) |
| `Core/NkSceneContext.h` | Ce que l'app fournit par frame : `lights` (copie de `NkLightDesc`), ambiance, IBL |
| `Tools/Environment/` | Ciel procédural / HDRI / soleil du ciel (`HostSkySunAsLight`) |

### 1.3 Modeleur (`Applications/NK3DModeler/src/NK3DModeler/`)

| Fichier | Rôle |
|---|---|
| `Viewport/NkDemo3D.cpp` | **Soumission des lumières utilisateur** (l. ~6285-6330) : position = nœud + gizmo, direction = −Y local de la rotation (quaternion), échelle → dimensions surfacique, température/exposition appliquées à la couleur. Sol infini suiveur de caméra (l. ~6560-6682). Façades du panneau : `Demo3DHostLightEx/SetLightEx` (l. ~14910), `SetUserSub` (l. ~15954 : changer le TYPE remet l'intensité au défaut du type), `SetLightAttMode`. Ambiance par défaut `kAmbientDef = 0.05` |
| `Shell/NkModelerScreens.h` | Panneau **Lumière** (l. ~6600-6800 : 4 boutons de type, couleur, puissance, loi, cônes, dimensions, ombres, source couleur/texture/mix), panneau **Rendu** (Occlusion ambiante, Ombres l. ~8380 : qualité/mise à jour/douceur/biais) |
| `Project/NkModelerAssets.h` | Persistance d'une lumière dans le `.nkscene` (écriture l. ~412-438, relecture l. ~551-570, champ `loi`) |
| `main.cpp` | **Crochets d'agent** (voir §5) et boucle de frame de l'éditeur |

---

## 2. LES CONVENTIONS PROUVÉES (mesurées à l'écran — ne pas re-dériver)

1. **NDC Z ∈ [0,1] sur TOUS les backends** (VK, DX11, DX12, ET OpenGL via
   `glClipControl`) : la correction `clipZ01` est composée dans `proj`/`viewProj`
   avant téléversement (`NkRender3D.cpp` l. ~2220-2245). La profondeur échantillonnée
   EST le z NDC.
2. **Reconstruction position depuis la profondeur** : `ndc = (2u−1, ndcYSign·(2v−1), depth, 1)`
   puis `invViewProj` — utiliser les matrices de `GetRenderInvViewProj()` (jitter + clipZ01
   compris). `ndcYSign` : **DX +1, GL/VK −1**. `yFlipUV` (orientation de la cible lue) :
   **VK −1, GL/DX +1**. Mesuré par le TAA sur trois backends.
3. **Ombre : comparaison `LESS_EQUAL`** (`NkDescs.h` l. 178), profondeur d'atlas **D32_FLOAT**
   qui **croît en s'éloignant de la lumière**. Donc « reculer le caster » = biais rasterizer
   **POSITIF**. Le signe négatif étend les ombres (faux contact « parfait ») et fabrique de
   l'acné proportionnelle à la pente — c'était le bug de l'acné, corrigé.
4. **`NkMat4f` est colonne-majeure** : `m[colonne][ligne]`. La translation est dans `m[3][*]`.
5. **DX11 tronque `depthBiasConst` en entier** (`NkDirectX11Device.cpp:1319`) — il faut des
   dizaines d'unités. `depthBiasSlope`/`Clamp` passent en flottant.

---

## 3. CE QUI EST DÉJÀ RÉSOLU — NE PAS Y RETOUCHER SANS RAISON

1. **L'acné « moiré diagonal »** avait DEUX sources, toutes deux corrigées :
   la SSAO v0 (comparaisons de profondeur sans normale — réécrite en v1 Alchemy)
   et le signe du biais rasterizer de la passe d'ombre (désormais +64/+4/+0.02).
   Validé par l'utilisateur : tous les biais du panneau à zéro, aucune acné, contact collé.
2. **Les modes de qualité d'ombre agissent** (PCF3/PCF5/Poisson lisent `globalCfg.y`,
   noyaux à compte constant, même largeur de pénombre). PCSS N'EST PAS ÉCRIT :
   replié sur Poisson, le combo du panneau le dit.
3. **La loi d'atténuation physique existe** (opt-in par lumière, `attenuationMode=1`) :
   1/d² fenêtré `(1−(d/r)⁴)²`, intensité = **WATTS** convertis par géométrie
   (ponctuelle P/4π, spot P/2π(1−cosθ), surfacique P/π). Défaut = loi héritée
   `(1−d/portée)²` (les scènes existantes sont réglées dessus).
4. **Le bloc de lumières a été étendu SANS casser les autres shaders** : `extras[32]`
   en QUEUE, et côté pbr.frag les remplisseurs sont TROIS SCALAIRES (`_pad0.._pad2`),
   pas un tableau (piège std140 : un tableau d'int a un pas de 16 octets — 48 au lieu
   de 12 — et tout se décale ; une tentative précédente a été REVERTÉE pour ça).
   `static_assert` sur l'offset côté C++ (`NkRender3D.cpp`).
5. **La garde du chemin directionnel** (slotCount), le biais normal EN TEXELS du tile,
   le biais du plan récepteur avec repli sur déterminant quasi nul : corrects, éprouvés.
6. **`FlushGraphRebuilds()`** : en mode partagé (l'éditeur possède la frame device),
   `BeginFrame` du renderer NE TOURNE JAMAIS — tout drapeau qu'il consomme est mort.
   Le bloc de « rejeu » du modeleur (NkDemo3D l. ~5141) doit consommer les drapeaux.
   C'était le bug du bouton SSAO sans effet.
7. **Culling des faces avant dans la passe d'ombre : REJETÉ DÉFINITIVEMENT** (la caméra
   d'un modeleur entre dans les objets — l'intérieur devenait éclairé). Ne pas retenter.

---

## 4. LES PROBLÈMES OUVERTS — c'est le travail demandé

| # | Symptôme constaté par l'utilisateur | Piste sérieuse |
|---|---|---|
| 1 | **Soleil (directionnelle) rend la scène NOIRE** à puissance 1000, là où elle devrait saturer en blanc | Vérifier la direction réellement soumise (rotation du nœud → −Y local, NkDemo3D l. ~6308), le type soumis, et l'ombre directionnelle (autoFit des cascades) qui pourrait couvrir tout à tort |
| 2 | **Spot : halo géant** couvrant l'écran à 10 000 W au lieu d'un disque de ~3 m (cône 35°, hauteur 4 m) | Hypothèse : c'est le BLOOM sur surexposition, pas le cône. Vérifier en coupant le bloom. Si le cône fuit réellement : `angles[i].x/y` (cos inner/outer) et la branche spot du shader |
| 3 | **« Effets lumineux décalés »** du point de lumière (flaque non centrée) | Peut être un artefact de perspective (disque de portée 10 m saturé). Vérifier avec portée 3 : le disque doit se recentrer sous la lampe. Sinon : position soumise vs gizmo (NkDemo3D l. ~6304-6307) |
| 4 | **Dimensions de la surfacique sans effet sur l'éclairage** | Elles sont désormais téléversées (`extras[i].yz`) mais RIEN ne les lit encore : implémenter **LTC** (Linearly Transformed Cosines, Heitz et al. 2016 — la méthode d'Unreal/Unity). Les tables de coefficients LTC se prennent TELLES QUELLES (jamais retapées à la main) |
| 5 | **Pénombre uniforme** (pas de durcissement au contact) | **PCSS** : nécessite un échantillonnage NON comparatif de l'atlas — absent du chemin DX11 (il faut lier l'atlas aussi comme texture ordinaire) |
| 6 | **Clignotement** (zones noires fugaces) en ORIENTANT une lumière ; disparaît à l'arrêt | Non diagnostiqué. Piste : réallocation/re-rendu des tuiles VSM pendant le mouvement (mise à jour « dynamique ») |
| 7 | **Une omni tronquée lit les tuiles d'une autre lumière** (>2 omnis ombrées : l'atlas 4096² ne tient que 2×6 faces de 1024²) | Correctif conçu : « 6 faces ou aucune » côté C++ (`AllocSlotsPoint`) + garde `slotCount < 6 → 1.0` côté shader omni. Le dire à l'écran |
| 8 | **Bloom et exposition caméra absents du panneau** (l'utilisateur ne peut pas les régler ; ils expliquent une partie des surprises visuelles) | Exposer au panneau Rendu comme l'« Occlusion ambiante » (utiliser `SetPostConfig` — le graphe se reconstruit tout seul via `FlushGraphRebuilds`) |

**Objectif final exprimé par l'utilisateur** : un éclairage direct et des ombres
« proches d'Unreal 5, donc corrects », comparables à Blender à réglages équivalents.
(Le GI complet type Lumen est HORS périmètre : le GI voxel actuel est un v0.)

---

## 5. LA BOUCLE DE VÉRIFICATION AUTONOME (déjà en place — s'en servir)

Des crochets d'environnement existent dans `main.cpp` (ils n'arment QUE ce que
l'interface arme déjà) :

```powershell
# Exemple : ouvrir le 1er projet récent, régler, capturer, sortir
$env:NK_OPEN_RECENT="0"        # ouvre le i-ème projet récent (différé : hôte prêt)
$env:NK_SHADOW_QUALITY="3"     # 0 aucune, 1 PCF3, 2 PCF5, 3 Poisson
$env:NK_SHADOW_SOFT="0.004"    # douceur
$env:NK_SSAO="1,0.5,1.0"       # occlusion : actif, rayon (m), intensité
$env:NK_LIGHT_ATT="1,1000"     # loi physique + puissance en watts sur toutes les lumières
$env:NK_MAT_SURFACE="0,0,0.5"  # vernis, rugosité vernis, diffusion (tous matériaux)
$env:NK_AGENT_SAVE="30"        # « Enregistrer tout » à la frame 30
$env:NK_AGENT_SHOT="60"        # capture « tutoriel » (fenêtre entière) à la frame 60
$env:NK_AGENT_EXIT="70"        # sortie propre à la frame 70
Start-Process .\Build\Bin\Release-Windows\NK3DModeler\NK3DModeler.exe -WorkingDirectory <racine du dépôt>
```

- Les captures atterrissent dans `captures/tutoriel_NNN.png` (racine du dépôt).
- Comparer deux captures par diff pixel (System.Drawing en PowerShell) sur la zone
  viewport `x∈[300,1550], y∈[320,690]` : le rendu est DÉTERMINISTE (bruit mesuré = 0),
  toute différence est un effet réel.
- ⚠ Si l'entraînement IA occupe le GPU, l'app tourne à ~4 i/s : caler les frames
  et les timeouts en conséquence (frame 60 ≈ 15-20 s).
- `NK_DUMP_HLSL=PBR` → écrit le HLSL réellement généré dans `Build/hlsl_dump/skin_fs.hlsl`
  (nom trompeur, contenu = le shader demandé).
- Autres crochets utiles : `NK_CAM_*` (caméra, scène de démo seulement), `NK_EDIT_*`
  (mode édition), `NK_FIX_CAM`, `NK_VP_ORTHO`.

---

## 6. LES PIÈGES QUI COÛTENT DES HEURES (tous vérifiés, tous payés)

1. **Cache de shaders** : la clé ne hache PAS les `#include` résolus. Après toute
   modification d'un `.glsli` : `rm -f Build/Bin/*/NK3DModeler/cache/shaders/*.nksc`,
   puis vérifier au journal `CompileVF 'PBR'` (et non « cache hit »).
2. **Chemin NkSL** : PBR/Shadow/Skybox compilent depuis `<Shader>/NkSL/*.nksl`.
   Les dossiers `DX11/`, `DX12/`, `MSL/` de ces shaders sont MORTS. `VK/` est la
   source de secours.
3. **`du` est un identifiant réservé** du générateur HLSL (X3003) — et un échec de
   `CreateShader` laisse la passe INERTE EN SILENCE. Toujours grep `X3003|\[ERR\]`
   dans `logs/app.log` après un nouveau shader.
4. **std140** : tableau de scalaires = pas de 16 octets par élément. Étendre un bloc =
   champs en QUEUE + remplisseurs scalaires + `static_assert` d'offset côté C++.
   Les shaders qui déclarent un bloc plus court restent corrects.
5. **Build** : `--target NKRenderer` AVANT `--target NK3DModeler` (sinon le binaire ne
   contient pas le correctif). Builder Debug ET Release, vérifier 29/29. **UN SEUL
   `jenga` à la fois dans le même arbre** (deux builds concurrents corrompent
   `Build/Obj` → binaires qui crashent absurdement ; si un crash survit à un revert
   du code : purger `Build/Obj/<config>` et rebuilder seul).
6. **Lancer l'app depuis la RACINE du dépôt** (sinon shaders absents, fenêtre noire),
   via PowerShell/cmd — JAMAIS Git Bash (les DLL msys du PATH font crasher au démarrage).
7. **Jamais PowerShell (`Get-Content`/`Set-Content`) pour réécrire une source** :
   l'UTF-8 accentué est double-encodé.
8. **Mode partagé** : `BeginFrame` du renderer ne tourne pas dans l'éditeur — tout état
   « consommé au BeginFrame » doit aussi l'être dans le bloc de rejeu (NkDemo3D l. ~5141).

---

## 7. RÈGLES DU DÉPÔT (impératives)

- **Français** partout : réponses, commentaires (qui expliquent le POURQUOI), journaux.
- **Aucune STL** : `NkString`, `NkVector`, allocateurs NKMemory. Pas de `new/delete` bruts.
- **Un correctif à la fois**, testé à l'écran (la boucle du §5) avant le suivant.
  « Ça compile » ne prouve RIEN. Jamais deux corrections dans le même build.
- **Ne jamais surévaluer** : dire ce qui ne marche pas. Un chantier « écrit mais non
  éprouvé » n'est pas livré.
- Commits : `./gitcommit.sh "message" <chemins explicites>` — jamais `git add -A`.
  Fermer NK3DModeler.exe avant de compiler (verrou de lien).
- Sources tierces : lire `docs/SOURCES_TIERCES.md` avant d'exploiter un article/du code
  (tables LTC : les prendre telles quelles, registre `THIRD_PARTY_LICENSES.md`).

---

## 8. LE PROMPT — à copier tel quel pour l'IA

```
Tu travailles sur le moteur 3D Nkentseu (C++ maison, zéro STL) et son modeleur
NK3DModeler, dépôt D:\Projets\2026\Nkentseu\Nkentseu, sous Windows/DX11.
Travaille et réponds EN FRANÇAIS.

LIS D'ABORD, dans cet ordre, le fichier LUMIERE_OMBRE_MISSION_IA.md à la racine
du dépôt (il contient la carte des codes, les conventions prouvées, les pièges
et la boucle de test), puis Applications/NK3DModeler/CARNET.private.md (entrées
des 8-9 août : tout ce qui a déjà été analysé, éliminé ou corrigé — ne refais
aucun diagnostic déjà tranché).

TA MISSION : vérifier et corriger les problèmes d'éclairage et d'ombre listés
au §4 du document, dans cet ordre de priorité :
1. Le Soleil (directionnelle) qui rend la scène noire — diagnostic puis correctif.
2. Le halo géant du spot surpuissant — trancher bloom vs fuite de cône, puis
   exposer Bloom et Exposition au panneau Rendu (modèle : la section
   « Occlusion ambiante » déjà en place, via SetPostConfig).
3. La garde « 6 faces ou aucune » des omnis (correctif déjà conçu au §4.7).
4. Les dimensions de la surfacique : implémenter LTC (Heitz 2016), les données
   arrivent déjà au shader dans uLights.extras[i].yz.
5. PCSS (durcissement au contact) — après avoir ajouté l'échantillonnage non
   comparatif de l'atlas côté DX11.
6. Le clignotement pendant l'orientation d'une lumière (diagnostic VSM).

MÉTHODE OBLIGATOIRE :
- UN correctif à la fois. Avant tout correctif, un TEST DISCRIMINANT qui coupe
  le sous-système suspect et prouve l'origine du défaut.
- Vérifie chaque correctif À L'ÉCRAN avec la boucle autonome du §5 (crochets
  NK_*, captures, diff pixel — le rendu est déterministe, bruit = 0), et grep
  logs/app.log pour X3003/[ERR] après tout changement de shader.
- Respecte les pièges du §6 à la lettre (cache de shaders, chemin NkSL, std140,
  ordre de build, un seul jenga, jamais Git Bash pour lancer, jamais PowerShell
  pour réécrire une source).
- Ne touche à rien de ce qui est marqué RÉSOLU (§3) sans preuve d'un défaut.
- Commits atomiques en français via ./gitcommit.sh avec chemins explicites,
  messages qui expliquent le POURQUOI. Ne jamais surévaluer un résultat :
  écris ce qui marche, ce qui ne marche pas, et ce que tu n'as pas pu tester.

LIVRABLE à chaque étape : le correctif commité + la preuve (captures avant/après
et/ou mesures de diff) + une entrée datée dans le CARNET.private.md du modeleur.
```
