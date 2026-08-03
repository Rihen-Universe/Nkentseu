# NK3DModeler — état des lieux (2026-08-02)

Document de reprise. Il dit **où en est le travail**, **ce qui reste**, et surtout
**ce qui est cassé avec tout ce qui a déjà été établi** — pour ne pas refaire les
mêmes recherches.

Dernier commit poussé : `9c379ee0` sur `main`.
Langue de travail : **français** (code, commentaires, échanges).

---

## 1. Règles de travail (impératives)

1. **Ne jamais piloter la souris ni le clavier de l'utilisateur.** Un incident a
   envoyé des clics synthétiques dans Blender. Interdiction définitive.
2. **Lancer l'application depuis la racine du dépôt** (`D:\Projets\2026\Nkentseu\Nkentseu`),
   sinon les shaders HLSL/GLSL et les icônes sont introuvables : fenêtre noire.
3. **Fichiers privés jamais poussés** (`CARNET.private.md`, `CLAUDE.md` est ignoré par git).
4. Commit + push à chaque palier stable, message en français expliquant *pourquoi*.
5. Un point à la fois, relance vérifiée (`Get-Process NK3DModeler`), l'utilisateur teste.

### PIÈGE DE BUILD — cause de deux faux diagnostics

`jenga build --target NK3DModeler` **ne reconstruit pas** NKWindow, NKRHI, NKRenderer :
ils sont liés tels quels. Toute correction dans ces bibliothèques exige de construire
leur cible **d'abord** :

```
jenga build --target NKWindow  --config Debug --platform Windows
jenga build --target NKRHI     --config Debug --platform Windows
jenga build --target NKRenderer --config Debug --platform Windows
jenga build --target NK3DModeler --config Debug --platform Windows
```

Un correctif absent du binaire donne **exactement la même image** qu'un correctif faux.

### PIÈGE — la valeur qui « fait foi »

Plusieurs réglages existent à deux endroits : un défaut de membre, et une valeur de
configuration réappliquée à l'initialisation. C'est la **seconde** qui gagne.
Exemple vécu : `NkRender3D::mIBLStrength = 0.05f` était écrasé par
`NkRendererConfig::ibl.iblStrength` via `NkRendererImpl.cpp:355`.

---

## 2. Architecture — repères

- **Hôte de la vue 3D** : `Applications/NK3DModeler/src/NK3DModeler/Viewport/NkDemo3D.cpp`
  (~9900 lignes), interface dans `NkDemo3DHost.h`. Tous les accesseurs `Demo3DHost*`.
- **Interface** : `Shell/NkModelerScreens.h` (~7700 lignes, panneaux),
  `Shell/NkModelerWidgets.h` (widgets), `Shell/NkModelerInput.h` (état + registre de
  zones), `Shell/NkModelerUI.h` (painter), `main.cpp` (boucle et ordre de peinture).
- **Nœuds** : `kNkvpMaxNodes = 160`. 0-85 objets de démo, 86-89 lumières de démo,
  90-95 empties, 96-159 objets utilisateur.
- **Shader PBR réellement utilisé** : `Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl`
  (chemin NkSL, prioritaire). Les `.hlsl` / `.vk.glsl` du même dossier **ne sont pas
  chargés** quand les `.nksl` existent — vérifié dans le journal :
  `[NkShaderLibrary] 'PBR' -> chemin NkSL (vrai dialecte)`.
  L'include partagé des ombres est `Resources/NKRenderer/Shaders/Include/NkShadowAtlas.glsli`.

### Routeur d'occlusion (gestion des événements GUI)

Modèle repris de NkCode (`NkGuiContext::PushOcclusion/PointReachable`), demandé
explicitement par l'utilisateur. Dans `NkHitRegistry` :

- `SetLayer(n)` / `LayerScope` : **0** panneaux, **50** menus et sous-menus, **100** modales.
- `PushOcclusion(rect, layer)` : emprise déclarée pendant le dessin ; la liste
  **consultée** est celle de la frame précédente → indépendante de l'ordre de dessin.
- `Reachable(pt)` : un point recouvert par une couche strictement supérieure est
  inatteignable. Consulté par `Add()` **et** disponible pour le code qui teste la
  souris à la main.
- L'ancienne garde `SetBlock`/`UiBlocks` **n'est plus utilisée depuis main.cpp** :
  deux mécanismes concurrents faisaient qu'un menu refusait ses propres clics.

---

## 3. Livré et validé par l'utilisateur

- Picker de couleur **modal** complet : roue chromatique, barre de valeur,
  Linéaire/Perceptuel, RVB/TSV, alpha, hexadécimal sRGB, Valider/Annuler
  (fermeture différée d'une image pour que l'annulation rende vraiment la couleur
  d'origine). Conversions via `NkColorF::ToHSV/FromHSV` de NKMath. Branché sur :
  couleur de lumière utilisateur, couleur de base du matériau, matériau du mesh,
  couleur d'ambiance, couleurs du ciel, couleur du brouillard.
- Navigateur de projet : barre de recherche + pastilles de filtre par type, sur un
  bandeau pleine largeur peint **après** les cartes (donc au-dessus).
- Barres de défilement unifiées (`editorkit::NkVScrollbar`) : hiérarchie, navigateur
  gauche et droite, propriétés. Gouttière opaque, harnais et flèches assombris via
  `NkScrollbarUserSkin`.
- Panneau **Rendu** : groupes Ambiance (intensité, couleur, source), Brouillard
  (actif, loi, couleur, densité ou début/fin), Ombres (qualité, mise à jour
  statique/dynamique, douceur, biais normal, biais de pente).
- Marge en haut des quatre sections du panneau de propriétés.
- Moteur : température/exposition des lumières, `GetEnvironment()` sur l'interface
  du renderer, `RefreshEnvironmentBindings()`, brouillard câblé jusqu'au shader,
  couleur d'ambiance dans le bloc de constantes caméra.

---

## 4. DÉFAUTS NON RÉSOLUS

### 4.1 — RÉSOLU (2026-08-02) — Trois faces d'un cube restaient éclairées

**Cause** : dans `NkMaterialSystem.cpp`, le repli de texture d'un emplacement non
renseigné était **blanc pour TOUS les slots**, normal map comprise. Le blanc est le
neutre des slots MULTIPLIÉS (albédo, ORM, émissif) ; ce n'est pas celui d'une
normale, dont le neutre est `(0.5, 0.5, 1)`.

Le shader lisait donc `nTs = (1,1,1)*2-1 = (1,1,1)` au lieu de `(0,0,1)`, et
calculait `N = normalize(T + B + geomN)` : une normale **inclinée de 54,7°**,
orientée différemment sur chaque face selon sa base tangente. **Tout objet sans
normal map explicite** était touché — ils retombent tous sur `Default_PBR`, et
`ob.normalStrength` vaut 1 dès qu'un matériau existe (`NkRender3D.cpp:1583`).

**Correctif** : repli dépendant du slot — `GetTexOr(name, fallback)`, blanc pour
albédo/ORM/émissif, `texLib->GetNormal1x1()` pour la normale.

**Ce qui a permis de trancher, et qui vaut pour la suite** : la vue NORMAL (touche Z)
comme instrument de mesure, mais avec les **valeurs de pixels lues sur la capture**,
pas jugées à l'œil. Le fait décisif : chaque canal ne prenait que **deux** valeurs
(~0,21 et ~0,79) et plusieurs faces avaient deux canaux saturés à la fois — or une
normale unitaire ne peut pas avoir deux composantes à 1. Signature de composantes
toutes égales à ±0,577, soit exactement `normalize(±T ±B ±geomN)`.
Puis une **sonde d'une ligne** dans le shader (afficher `geomN` au lieu de `N`) a
séparé l'étage vertex de l'étage fragment : `geomN` sortait signée et correcte.

Les pistes 1, 3 et 4 du diagnostic d'origine ont été fermées avec preuve (le cube
passe bien par le shader PBR ; la grille de voxels n'est bâtie que sous `NK_GI_TEST`
donc inerte ; les types de lumière sont corrects de bout en bout). La piste 2
(matcap) a été écartée après mesure. La cause était ailleurs qu'aux quatre endroits
soupçonnés.

### 4.1 bis — ANCIEN TEXTE (conservé pour mémoire)

**Symptôme** : avec un **soleil** dirigé vers le bas, trois faces visibles du cube
sont éclairées au lieu de la seule face du dessus. Pire : une **point light placée
en haut éclaire aussi le bas**. Les faces concernées ne changent pas quand on tourne
la vue → dépend de la normale **monde**, pas de la vue.

**Ce qui a été éliminé** :
- Ce n'est pas l'intensité de l'ambiance seule (passée de 0.3 → 0.05 aux deux
  endroits : `NkRendererConfig.h` et `NkRender3D.h`).
- Ce n'est pas l'IBL directionnel seul : la source d'ambiance « Couleur unie »
  force `uCam.iblColor.w = 0` et le shader prend alors `irr = vec3(1.0)`,
  `pref = vec3(1.0)` — donc uniforme.
- La normale semble correctement transformée (`pbr.vert.nksl:102`, `vNormal = N_real`).
- Le shader applique bien `NdL = max(dot(N,L), 0.0)` (pas de `abs`).

**Pistes à vérifier, dans cet ordre** :
1. **Le cube utilisateur est-il rendu par le shader PBR ?** Vérifier quel matériau
   `HostMatHook` / `Demo3DHostMeshMaterial` lui assigne. S'il passe par un matériau
   « masked » ou un autre shader, tout le raisonnement ci-dessus est hors sujet.
2. **`uCam.viewMode`** : en mode SOLID/matcap, la couleur vient d'une matcap
   échantillonnée par la normale — ce qui produirait exactement ce symptôme. Le HUD
   annonce `Affichage(Z): RENDERED`, mais vérifier ce qui est réellement écrit dans
   le bloc caméra (`cb.viewMode`), pas ce qu'affiche le HUD.
3. **`NkComputeVoxelGI`** (`Include/NkVoxelAO.glsli`) : le terme `voxGI.xyz` s'ajoute
   à l'ambiance. Si la grille de voxels contient autre chose que zéro, il éclaire
   selon la position/normale. Le neutraliser temporairement pour trancher.
4. **Le point light qui éclaire le bas** : vérifier l'atténuation `att` et surtout
   que `positions[i].w` (le type) vaut bien 1. Si le type lu vaut 0, la branche
   directionnelle s'applique et `L` devient constant — ce qui éclairerait
   « le haut et le bas » selon la direction, exactement le symptôme décrit.
   **C'est la piste la plus prometteuse pour ce cas précis.**

**Méthode qui a marché** : demander un rendu **sans aucune lumière** dans la scène.
C'est ce test qui a révélé l'ambiance à 0.3. Refaire ce genre de test isolant.

### 4.2 — RÉSOLU (2026-08-02) — Le ciel procédural ne produisait aucun effet

**Trois causes distinctes, aucune là où on les cherchait.**

1. **Le ciel n'était jamais AFFICHÉ.** `SetSkyboxEnabled` existe dans le moteur et
   le shader `Skybox` est compilé au démarrage, mais **aucune ligne de
   l'application ne l'appelait**. Corrigé : `Demo3DHostSetSkyVisible` + case
   « Afficher le ciel » (cf. 4.2 bis).
2. **Il était peint à 5 % de sa luminosité.** `skybox.frag.nksl` multipliait son
   échantillon par `uCam.iblStrength` — l'intensité de l'**ambiance**, volontairement
   basse (0,05) pour que l'environnement ne délave pas les objets. Le ciel sortait
   quasi noir et paraissait absent alors qu'il était correctement généré et rendu.
   Corrigé : `SetSkyIntensity` (`viewOpts.y`), réglage **Luminosité** propre au ciel.
3. **Il n'existait qu'un dégradé à trois couleurs** — pas de soleil, pas de nuage,
   pas de diffusion. Ce n'était pas cassé, ce n'était pas implémenté. Ajouté :
   modèle **Physique** (Preetham 1999) et couche de **nuages** procéduraux.

**La leçon, qui est revenue trois fois dans la soirée** : deux grandeurs différentes
portées par un seul nombre. « Combien l'environnement ÉCLAIRE » n'est pas « à quel
point le ciel SE VOIT ». À chaque fois, l'un des deux réglages devenait inutilisable
sans que rien ne le signale.

### 4.2 ter — Anciennes notes de diagnostic (conservées)

`Demo3DHostApplySky()` appelle `env->LoadProcedural(top, horizon, ground)` puis
`r3->RefreshEnvironmentBindings()`. Aucun changement visible.

**À vérifier** :
- `LoadProcedural` recrée-t-il les textures (nouveaux handles) ou écrit-il dans les
  existantes ? Si les handles sont identiques, le rebinding est inutile et le
  problème est ailleurs (contenu non écrit, ou cache IBL qui court-circuite).
- Le journal contient-il une trace de `LoadProcedural` ? Ajouter un log du handle
  avant/après.
- La source d'ambiance doit être sur « Ciel procédural » pour que
  `uCam.iblColor.w = 1` — sinon le shader ignore la cubemap **par construction**.
  Vérifier que `st.envSource == 1` appelle bien `Demo3DHostSetAmbientUseEnv(true)`.

**Deux pistes ÉLIMINÉES (2026-08-02), ne pas les refaire** :

- *Le cache IBL court-circuiterait l'écriture* — **faux**. `IBLHash`
  (`NkEnvironmentSystem.cpp:41`) intègre bien `skyTop / horizon / ground`, en plus
  des tailles et de la version. Deux appels qui rechargent le même
  `nk_ibl_a91b913f.bin` signifient simplement deux appels avec les mêmes couleurs.
- *`LoadProcedural` recréerait les textures, rendant le rebinding nécessaire* —
  **faux**. Il écrit dans les textures EXISTANTES (`WriteTextureRegion` /
  `WriteTexture`, `NkEnvironmentSystem.cpp:498` et suivantes) : les handles ne
  changent pas et le contenu part réellement au GPU.

Le contenu du ciel est donc bien calculé et téléversé. Ce qu'il restait à faire était
de le **rendre visible** (cf. 4.2 bis, fait) ; s'il subsiste un défaut d'**éclairage**
par le ciel, il faut chercher du côté de `mIBLUseEnv` et de la lecture de la cubemap
d'irradiance par le shader, pas du côté de la génération.

### 4.2 bis — Le ciel n'est JAMAIS visible dans la scène (manque distinct)

À ne pas confondre avec le défaut ci-dessus. Ce sont **deux mécanismes séparés** :

| Effet | Réglage moteur | État |
|---|---|---|
| Le ciel **éclaire** les objets (ambiance, reflets) | `NkRender3D::SetIBLUseEnv(true)` | exposé par le combo « source d'ambiance » |
| Le ciel est **visible** en fond de scène | `NkRender3D::SetSkyboxEnabled(true)` / `NkRendererConfig::ibl.drawSkybox` | **jamais activé** |

`drawSkybox = false` par défaut (`NkRendererConfig.h:225`) et **NK3DModeler n'appelle
jamais `SetSkyboxEnabled` ni ne renseigne `drawSkybox`** — aucune occurrence dans
toute l'application. Le ciel ne peut donc, par construction, qu'agir sur l'éclairage :
son invisibilité n'est pas un bug de rendu mais une fonctionnalité absente.

Dans Blender les deux vont ensemble.

**FAIT (2026-08-02)** : `SetSkyboxEnabled` est câblé
(`Demo3DHostSetSkyVisible` / `Demo3DHostSkyVisible`) et exposé dans le panneau
**Rendu** par une case « Afficher le ciel », placée sous la source d'ambiance.

Les deux réglages restent **volontairement indépendants** : « d'où vient la
lumière » et « qu'est-ce qu'on voit derrière » sont deux questions distinctes. On
éclaire couramment une scène avec un HDRI sans l'afficher en fond, et on affiche
parfois un ciel qui ne pilote pas l'ambiance. Les lier aurait été une facilité
payée plus tard.

### 4.3 — Fermeture de l'application à la RESTAURATION d'une fenêtre réduite

**Non résolu.** Voir aussi la section 9 de `CLAUDE.md`.

Symptôme : l'application meurt en restaurant une fenêtre minimisée. Le journal
s'arrête net sur `ResizeSwapchain` puis `RebuildRenderGraph`.

**Correctifs déjà appliqués (à conserver — chacun est un vrai défaut)** :
- `NkRendererImpl::BeginFrame` fait désormais l'auto-resize **avant**
  `mDevice->BeginFrame` : on détruisait les cibles et on reconstruisait le graphe
  au milieu d'une frame ouverte.
- `WaitIdle()` avant `RebuildRenderGraph` dans `ApplyRenderSize`.
- `NkDirectX11Device::OnResize` : no-op si la taille est inchangée (DX12 l'avait déjà).
- `mWidth/mHeight` posés **après** `ResizeSwapchain` (sinon la trace « avant: » ment).
- Boucle principale : `window.GetSurfaceDesc()` à zéro → `Sleep(8)` + `continue`.
  **`GetSize()` ne convient pas** : une fenêtre réduite garde sa taille logique.
- `WM_SIZE` : pas de `RDW_UPDATENOW` quand la taille est nulle.

**Pistes non explorées** : ordre de destruction des vues (RTV/DSV) encore liées au
contexte immédiat ; `framesInFlight = 3` sur un contexte DX11 immédiat ;
comportement de `IDXGISwapChain::ResizeBuffers` en mode flip après passage par une
surface nulle. Envisager un test avec `framesInFlight = 1`.

### 4.4 — Lumière surfacique (Area) : sans ombre ET sans direction

**Deux manques distincts, même origine — le type 3 n'est traité nulle part.**

1. **Pas d'ombre** : `AllocSlotsForLights` (`NkVirtualShadowMaps.cpp:543`) fait
   `case NK_AREA: break;`. Piste : la traiter comme un spot large (une tuile).
2. **La direction n'a aucun effet** (constaté par Rihen, 2026-08-02). Dans
   `pbr.frag.nksl`, la boucle des lumières a une branche `lt == 0` (directionnelle),
   `lt == 1` (ponctuelle, cookie cube) et `lt == 2` (spot, cône + cookie), mais
   **aucune branche `lt == 3`**. Une surfacique retombe donc dans le cas général
   « point » : `L = normalize(pos - worldPos)`, atténuation radiale, et sa direction
   n'est jamais lue. Le CPU la transmet pourtant (`NkDemo3D.cpp:4558`) et le gizmo
   la fait tourner — le retour visuel existe, l'effet non.
   À faire : un vrai terme surfacique (au minimum un `max(dot(-L, dirAire), 0)` pour
   n'émettre que d'un côté du panneau, idéalement une LTC).

### 4.6 — Spot : l'ombre disparaît quand on monte la lumière très haut

Pas un défaut de l'atlas : `AllocSlotsSpot` prend `far = light.range`
(`NkVirtualShadowMaps.cpp:448`), et le shader coupe l'atténuation avec la même
valeur (`att = max(1 - dist/range, 0)`). Au-delà de sa **portée**, un spot n'éclaire
plus *et* ne projette plus — les deux s'éteignent ensemble, ce qui est cohérent mais
illisible pour l'utilisateur.

À décider : soit la portée s'ajuste toute seule à la scène, soit elle devient
visible dans le panneau (avec son gizmo), pour qu'on comprenne *pourquoi* l'ombre
part. La deuxième option est plus proche de Blender.

**Rappel utile (pas un défaut)** : l'ombre d'un **soleil** garde la même taille quand
on déplace l'objet — les rayons d'une directionnelle sont parallèles. Seules les
ponctuelles et les spots font varier la taille avec la distance.

### 4.7 — L'ombre du soleil est nettement trop petite

Constaté par Rihen (2026-08-02), **distinct du point ci-dessus** : la taille ne doit
pas varier avec la distance (c'est correct), mais la surface couverte est bien plus
petite que ce que le cube devrait projeter.

**Où chercher** : `ComputeDirectionalCascade` (`NkVirtualShadowMaps.cpp`) — c'est lui
qui cadre le volume orthographique de la cascade. Deux éléments de contexte utiles :
le modeleur force **une seule cascade** (`cfg.shadow.cascadeCount = 1`, `NkDemo3D.cpp`
vers la ligne 7842), et l'étendue est dérivée du frustum caméra entre `cascadeNear` et
`cascadeFar`. Avec une seule cascade couvrant toute la profondeur de vue, le volume
est énorme et un tile de 1024 px donne un texel très grossier.

Hypothèses à départager, dans cet ordre :
1. l'étendue ortho est calculée sur un frustum trop large → l'ombre existe mais fait
   quelques texels, donc paraît minuscule ;
2. le **biais normal** (0,050 par défaut dans le panneau Rendu) érode l'ombre : sur un
   texel grossier, décaler le point d'échantillonnage le long de N ronge les bords ;
3. l'ombre est correctement dimensionnée mais **rognée** par le volume (le caster sort
   partiellement du tronc).

Test isolant qui tranche vite : mettre le biais normal à 0 et regarder si la taille
change. Si oui → piste 2. Sinon → piste 1, et il faut regarder le volume calculé.

### 4.5 — Ombres : seul le chemin Vulkan lit l'atlas par lumière

`Resources/NKRenderer/Shaders/PBR/` : **VK** et **NkSL** échantillonnent l'atlas
per-light. **DX12, OpenGL et MSL** calculent une seule ombre (cascade du soleil) et
l'appliquent à toutes les lumières. À porter si l'on ajoute le choix de backend.

---

## 4bis. CIEL — état au 2026-08-02 et suite demandée

**Fait** : ciel visible **évalué dans le shader** (`skybox.frag.nksl`, bloc de
paramètres binding 5), modèles **Dégradé** et **Physique** (Preetham 1999), **nuages
procéduraux animés**, soleil en élévation/azimut, teinte, disque, lien vers un soleil
de la scène (plusieurs sources possibles), soleil manuel capable d'éclairer la scène,
boutons de remise à zéro.

### Trois contraintes multi-backend apprises à la dure (VK / GL / DX11 / DX12 / Metal)

1. **Aucun identifiant ne doit différer d'un autre par la seule casse.** GLSL les
   distingue, le passage NkSL → HLSL non : `float y` à côté de `float Y` donne
   « error X3003: redefinition of 'y' ». Le shader ne compile pas, le pipeline n'est
   pas créé, et **le ciel disparaît sans aucun message à l'écran**.
2. **Retour unique dans chaque fonction.** Un `return` anticipé devient en HLSL une
   variable de résultat non écrite sur tous les chemins
   (« X4000: potentially uninitialized »), donc une valeur indéterminée qui peut
   différer d'un backend à l'autre.
3. **DX11 (SM5) n'a que 14 emplacements de constant buffer (b0..b13).** Un bloc
   uniforme au-delà passe sur VK/DX12/GL/Metal et échoue sur DX11
   (« X4567: maximum cbuffer exceeded »). Les blocs uniformes et les textures ont des
   espaces de registres distincts (b# / t#) : un même numéro peut servir aux deux.

### Fait depuis (2026-08-03)

- **Ciel évalué dans le shader**, donc immédiat et animable ; la cuisson ne sert
  plus qu'à l'éclairage.
- **Nuages animés** (réglage Vitesse) et **échantillonnage sphérique** : la
  projection planaire `d.xz / d.y` envoyait toutes les directions du zénith sur un
  **point unique** du bruit — les nuages y convergeaient en tourbillon. Un bruit 3D
  sur la direction n'a aucun point dégénéré.
- **Étoiles** : disque à centre tiré dans la cellule (la première version allumait
  la cellule entière, d'où des **carrés**), magnitude en loi de puissance —
  beaucoup de petites, quelques grosses. Elles s'effacent seules quand le ciel
  s'éclaire, donc un cycle jour/nuit les révélera sans qu'on les pilote.
- **Lunes (0 à 2)** : élévation, azimut, taille, luminosité, couleur. Leur **phase
  se déduit du soleil** par défaut — la lune est une sphère qu'il éclaire, le
  croissant sort du calcul. Une option **« Phase forcée »** permet de l'imposer :
  c'est un choix de mise en scène, déclaré et non subi.

- **Troisième modèle : Atmosphère (Rayleigh + Mie)**, diffusion simple intégrée
  le long du rayon, 12 pas de vue × 5 pas de lumière. Écrit **depuis la
  physique** — chaque terme se vérifie, contrairement à un modèle tabulé.
  Il apporte ce que Preetham ne sait pas faire : le **soleil sous l'horizon**.
  Le test `altL < 0` (le trajet vers le soleil traverse la Terre) éteint les
  couches basses avant les hautes — c'est de là que sort le crépuscule.
  Son facteur d'échelle (22) est calé pour qu'un ciel de midi rende comme
  Preetham : changer de modèle ne doit pas faire sauter l'exposition de la scène.

- **Ciel étoilé en mouvement** : **Rotation** (rad/s) fait dériver la voûte
  entière — le champ est fixe en espace monde, c'est la *direction* qu'on tourne
  avant de l'échantillonner. Et **Filantes / min**.

  Les étoiles filantes sont **tirées du temps**, jamais d'un générateur
  aléatoire : le temps est découpé en créneaux, chaque créneau hache son numéro
  pour en déduire départ, direction et inclinaison. Une même seconde redonne donc
  **toujours** la même filante — une capture se rejoue à l'identique et un rendu
  par images se recolle sans sauts. Un tirage aléatoire par image aurait rendu
  tout rendu différé impossible.

### Lunes en nombre libre (demandé pour les courts métrages)

Le tableau est dimensionné à **2**. Passer à 4, 8 ou davantage ne demande que deux
`vec4` de plus par lune dans le bloc caméra et une ligne d'interface : la boucle,
elle, ne change pas — « une » et « plusieurs » suivent déjà le même chemin.
À faire quand un film le demandera, pas avant : chaque lune coûte 32 octets de bloc
caméra, et ce bloc est déjà à 672 octets.

### Demandé, pas encore fait

- **Activer / désactiver l'animation à volonté** (geler les nuages sans perdre leurs
  réglages) — un simple interrupteur, la vitesse existe déjà.
- **Nuages sombres de pluie** : la couleur des nuages existe, il manque un
  assombrissement lié à leur **épaisseur** (un nuage dense doit s'auto-ombrer par en
  dessous), et une couverture qui monte au-delà d'un simple seuil.
- **Ciels d'ambiance** (désert, orage, brume…) : ce sont des **préréglages** — un jeu
  de valeurs nommé, pas du code. À traiter avec le fichier de données, pas en dur.
- **Hosek-Wilkie — FAIT (2026-08-03)**, quatrième entrée du combo Modèle, grâce à
  la distribution officielle 1.4a récupérée par Rihen (dossier local
  `C:\Users\Rihen\Documents\revu`). Tables copiées **verbatim** — empreinte
  SHA-256 vérifiée à la copie, en-tête BSD intact, attribution dans
  `THIRD_PARTY_LICENSES.md`. Architecture : le CPU cuit les tables en
  9 coefficients par canal (`NkHosekCookRGB`), le shader n'évalue que la formule
  — dix vec4 descendent au GPU, jamais les 65 Ko. Le **sol de la scène sert
  d'albédo**. **Normalisation sans œil** : une référence (soleil à 47°, même
  turbidité) est cuite et évaluée au zénith côté CPU, l'échelle l'amène à 1,0 —
  changer de modèle ne fait pas sauter l'exposition, et le cycle du jour garde sa
  dynamique. Comme Preetham, pas défini sous l'horizon : fondu crépusculaire sur
  ~8°, l'Atmosphère reste le modèle des couchants.

- **(Historique — le blocage résolu par le dossier `revu`.)** Le modèle repose sur un jeu de
  **coefficients tabulés** (`ArHosekSkyModelData_RGB`, plusieurs centaines de
  flottants). Sa licence ne fait pas obstacle : **BSD 3-clauses**, usage
  commercial permis, à la seule condition de conserver la mention de copyright
  (Lukas Hosek, Alexander Wilkie, 2012-2013) et de ne pas se servir du nom des
  auteurs pour promouvoir le produit.

  Ce qui a fait renoncer, c'est la **fiabilité de la transcription**. Deux
  lectures indépendantes du fichier ont rendu la **même séquence de valeurs**
  mais avec un **décalage d'indexation de trois positions**. Dans une table où la
  position porte tout le sens, un tel décalage corromprait l'ensemble en silence
  et produirait un ciel *plausible mais faux* — le pire résultat possible.

  **Pour l'ajouter** : récupérer le fichier officiel tel quel (ne pas le
  retaper), le déposer dans `Resources/NKRenderer/Sky/`, garder son en-tête de
  licence intact, et écrire l'évaluation (Perez étendu à 9 coefficients) autour.
  Le combo Modèle est déjà prêt à recevoir une quatrième entrée.

  ⚠ **Ne jamais « réécrire à sa manière » les tables pour effacer l'origine** :
  l'algorithme est libre, les données restent sous licence.
- **Lunes** (une ou plusieurs) et **étoiles** : termes additifs dans le shader, une
  fois le ciel calculé en temps réel — ce qui est désormais le cas.
- **Animation des paramètres** : maintenant qu'ils descendent au GPU à chaque image,
  il suffit qu'une piste écrive dans les mêmes variables. Le rôle de thème
  `nk3d.parametre_animable` existe déjà.

### Le ciel visible est en avance sur le ciel qui éclaire

Le visible est calculé par image ; l'**éclairage** (irradiance + reflets) reste cuit
en cubemaps et n'est refait qu'à la demande. Bouger le soleil en continu fait donc
diverger les deux.

**C'est soluble, par trois voies de coût croissant :**

1. **Irradiance ANALYTIQUE.** Le ciel est désormais une formule : on peut en déduire
   directement les 9 coefficients d'harmoniques sphériques qui décrivent l'éclairage
   diffus, sans aucune convolution. Quelques dizaines d'opérations par image, sur les
   cinq backends, sans compute shader. **C'est la voie recommandée** pour l'ambiance.
2. **Reflets à cadence réduite** : la cubemap spéculaire est refaite toutes les N
   images plutôt qu'à chaque. Invisible sur un cycle lent, très bon marché.
3. **Convolution GPU à chaque changement** (`NkIBLCompute`, déjà présent). Bloquée
   sur DX11 : `fxc cs_5_0` donne un écart max de 175/255 sur ~0,8 % des texels, là où
   GL/VK/DX12 sont à 5/255 près. Réparer ce noyau est un chantier en soi.

## 5. Reste à faire — ordre décidé par l'utilisateur

L'ordre a été fixé explicitement : **modélisation d'abord**, la sauvegarde
« viendra avec le temps ».

1. **Matériaux** (le gros morceau, prévu et repoussé) :
   - Pastille **Matériaux** dédiée, visible **uniquement** pour model et mesh —
     jamais pour caméra, lumière ou empty. Liée à l'objet sélectionné.
   - **MOTEUR** : emplacements de matériaux multiples par model + table par face +
     rendu en sous-appels. `NkDrawCall3D::materialSlots` existe déjà (phase M.8).
     C'est le socle du bouton « Assigner » et la matérialisation du concept de mesh.
   - Aperçu de matériau (rendu hors écran).
2. **Sélecteur de fichier / dossier** : `Engine/NKEditorKit/src/NKEditorKit/NkFilePicker.h`
   existe (utilisé par NkCode). À brancher sur le champ HDRI, ou réécrire au style du
   modeleur si son contrat ne s'y prête pas. **Demandé, pas encore fait.**
3. **Textures de lumière** : mode Couleur / Texture / Mix pour **toutes** les
   lumières. Règle : **vraies textures fournies par l'utilisateur** ; si aucune n'est
   fournie (mode Texture comme mode Mix), on **conserve uniquement la couleur**.
   Un emplacement en mode Texture, plusieurs en mode Mix.
4. **Réglages** : choix du backend graphique (DX11, DX12, Vulkan, OpenGL, Metal).
   Suppose une relance du contexte de rendu → réglage persistant appliqué au
   démarrage, et n'offrir que les backends de la plateforme.
5. Caméras, éclairage (suite), import de modèles, combo « Ajouter », mode Édition.
6. Sortie des 96 objets de démo de la scène (en dernier).
7. Listes restantes du panneau : Groupes de Vertex, Shape Keys, Maps UV, Attributs
   de Couleur, Attributs ; Espace Texture ; Données Géométrie.
8. Pipette dans le picker de couleur (comme la maquette Blender).
9. HDRI : à terme, aller les chercher **dans le projet** plutôt qu'un chemin saisi.

---

## 6. Sémantique Model / Mesh (définition de l'utilisateur)

> « un mesh = l'ensemble des éléments graphiques (vertices, edges, faces) d'un model
> qui partagent les mêmes propriétés, qu'ils soient directement connectés ou pas »

Donc **un mesh EST un emplacement de matériau**. Model = parent unique ; ses enfants
sont tous frères ; un mesh n'est jamais parent d'un autre mesh.
Implémenté dans `NkDemo3D.cpp` : `nkvpIsMesh[]`, `nkvpIsModel[]`,
`Demo3DHostModelRootOf`, `FlattenModel`, point de passage unique dans
`Demo3DHostSetNodeParent`.

Drapeaux **par contexte** : `nkvpObjHidden/Locked` (scène) contre
`nkvpMeshHidden/Locked` (model). Visibilité = OU en scène (propagation model→scène) ;
verrou jamais propagé.

---

## 7. Leçons de conception (acquises à la dure)

- Un état qui se propage doit être **affiché sous sa forme effective**, pas sous sa
  forme locale (le cadenas ouvert alors que la sélection était refusée).
- Quand on remplace un mécanisme, **retirer l'ancien**. Deux gardes concurrentes ont
  fait qu'un menu refusait ses propres clics.
- Un invariant se pose au **point de passage unique**, jamais dans une branche « sinon ».
- Ne pas déduire un état structurel de la forme de l'arbre.
- `Outline` repeint le fond : utiliser `OutlineSharp` pour un contour seul.
- Les listes déroulées retiennent un **pointeur** sur la valeur et l'écrivent à la
  frame suivante : ce pointeur ne doit **jamais** désigner une variable locale.
- Un champ de saisie ne doit jamais écrire son texte d'invite dans le buffer.
