# NKRenderer — Plan de finition (Phase A du cap éditeurs)

> Suivi partagé. Rihen + agent rendu (cet agent) finit NKRenderer ; un autre
> agent tient NKCode + NKEditorKit. **Ne pas modifier** : `Engine/NKEditorKit`,
> `Applications/NKCode`, `NKGui`. NKReflection = ✅ terminé (5/5 phases).

## A1 · Bugs concrets (correctness) — PRIORITÉ
- [x] **A1.1 — DX11 ghosting** — NON REPRODUIT (2026-06-26). Demo3D (PBR, caméra
      orbitante) ET DemoGLTF (viewer) nets en DX11, aucune traînée. Probablement
      déjà résolu depuis la note 06-23. Re-vérifier si un cas précis ressurgit.
- [x] **A1.2 — demo5 sombre** — PAS un bug renderer (identique GL=DX12, ombre OK).
      Cause : sphères 100% métalliques (no diffus) + `drawSkybox=false` + IBL 0.3
      → reflètent un environnement sombre/invisible. Fix DEMO : HDR piazza +
      drawSkybox + iblStrength=1.0 (comme Demo4). Matériaux désormais visibles.
- [x] **A1.3 — DX12 résidus d'ombres** — acné NON significative (2026-06-26).
      Contact shadows doux et propres sur Demo3D DX12, pas de striping/moiré.
      Acceptable ; ré-évaluer si un cas précis montre de l'acné.
- [~] **A1.4 — Vulkan chute FPS en Debug** — À VÉRIFIER EN RELEASE (probable
      surcoût validation layers Debug, pas un vrai bug). Différé (build Release).

## A1.6 · Vulkan blanc/hang plein écran — RÉSOLU par REVERT (2026-06-27)
- [x] **VRAIE CAUSE = régression de session, pas un bug d'origine.** Test décisif :
      l'état du **dernier commit rend VK plein écran proprement** (0 erreur, 80 frames,
      shutdown OK) ; les changements de rendu NON-COMMITÉS faisaient **hang GPU** au
      plein écran (deadlock : un submit attend une sémaphore swapchain jamais signalée
      → `mCmd` jamais terminé → `mCmd->Reset()` de la frame suivante spin dans le driver
      NVIDIA → device lost). Diag : gdb ATTACHÉ au process hung (pile `BeginFrame:793
      → NkVulkanCommandBuffer::Reset` spin) + VK validation.
      **Coupable = le câblage FXAA RenderGraph + split post-process** (passe `FXAA_Final`
      + transient `ToneLDR`) dans `NkRendererImpl` / `NkPostProcessStack`.
      **FIX retenu (décision Rihen) : REVERT de `NkRendererImpl.cpp` +
      `NkPostProcessStack.cpp/.h` au dernier commit** (= pas de FXAA, post-process
      baseline). On GARDE la **refonte skinning** (`NkRender3D` : bones SSBO→UBO ring +
      pipeline Skin) qui rend VK+GL, les 7 loaders, et les fixes shaders (double-gamma,
      lumière skin). VK demo13/demo12 plein écran : exit propre, 0 hang.
- Écartés par bisection exhaustive (NE causaient PAS le hang) : la sync frame VK
  (inchangée vs commit), `NkRender3D`/skin draw, l'alignement framesInFlight=3, la
  dépendance de sous-passe depth (WAW), les shaders GLSL-as-SPIRV (présents aussi au
  commit), la sync-validation. OpenGL rendait la refonte sans hang dès le départ.
- **Pièges méthodo** : (1) tester en fenêtre **0×0 minimisée** masque le hang (pas de
  vrai travail GPU) → toujours fenêtre **normale** ; (2) le sink fichier du logger
  (`logs/app.log`) tourne sur un thread de fond → contient tout jusqu'au hang même après
  kill ; (3) gdb ne peut pas LANCER l'exe ici (DLL ucrt64) mais peut s'ATTACHER ;
  (4) `logs/app.log` est APPEND → filtrer par date/marker.
- **Note hardware (Rihen)** : le MSI s'éteint sec sous calcul GPU lourd (lumières/Cycles,
  pareil Blender/Unity/Unreal) = protection alim/thermique du laptop, PAS notre moteur.
  Remèdes : Power Limit ~80% (MSI Afterburner), undervolt GPU, adaptateur d'origine,
  diag temps via HWiNFO64. Distinct du hang logiciel ci-dessus.
- Outil ajouté : `NK_MAXFRAMES=<n>` (main.cpp) → sortie propre après n frames.
  `NK_VK_VALIDATION=1` → validation routée vers NkLogger.

## A1.7 · DX11 écran noir / DX12 pas de modèle — codegen NkSL→HLSL (DIAGNOSTIQUÉ)
- [ ] **DX11 — Skin shader X3014** (`incorrect number of arguments to numeric-type
      constructor`, ligne 92 du HLSL généré). Cause : `skin.vert.nksl` utilise
      `mat4(1.0)` (identité) et `mat3(mat4)` (troncature) — valides en GLSL, INVALIDES
      en HLSL (`float4x4(1.0)` / `float3x3(float4x4)` interdits). Le transpileur NkSL
      « vrai dialecte » ne les traduit pas. **PBR/Shadow compilent OK** (pas de
      skinning) → seuls les modèles SKINNÉS sont noirs en DX11, pas toute la scène.
      Aussi : 1× `CreateBuffer hr=0x80070057` à l'init (à investiguer, mineur).
      Fix options : (a) transpileur NkSL→HLSL gère `matN(scalar)`/`matM(matN)` —
      correct mais composant PARTAGÉ ; (b) workaround dans skin.vert.nksl (identité
      explicite + `mat3(m[0].xyz,m[1].xyz,m[2].xyz)`) — bas risque, ne corrige que Skin.
- [ ] **DX12 — pas de modèle** : `CreateInputLayout` attend `TEXCOORD/2` et `/3`
      mais la déclaration ne les fournit pas → `CreateGraphicsPipelineState`
      hr=0x80070057. Décalage signature VS (HLSL généré) vs input layout. Codegen
      NkSL→HLSL (SM6/DXC) distinct du bug DX11.

## A2 · Finitions post-process (fort ROI / faible coût)
- [ ] **A2.1 — FXAA wiring RenderGraph** — À REFAIRE PROPREMENT : c'était la cause du
      hang VK plein écran (A1.6), donc REVERTÉ. Le split tonemap→ToneLDR transient +
      passe FXAA_Final déclenchait un deadlock sémaphore swapchain. Quand on y revient :
      investiguer la coordination acquire/submit/present quand une passe transient
      color-only s'insère juste avant le présent. À FAIRE APRÈS commit du skinning stable.
- [ ] **A2.2 — Auto-exposure** (~1-2 h, adaptation luminance moyenne)
- [ ] **A2.3 — TAA** (~4-5 h, optionnel, gros impact « next gen »)

## A1.5 · « Lumière persistante » sur modèles chargés (demande Rihen)
- [~] **REPRODUIT de façon fiable** (NK_FREEZE fige l'anim, ajouté à DemoSkin) :
      lueur **irisée multicolore** (vert/bleu/jaune/rose) sur le SOL sous les
      pieds de CesiumMan/BrainStem ; Fox (brun) = sol propre.
      **Isolé** au terme **Lo (éclairage direct) du shader SOL** (pbr.frag) :
      amb seul = gris propre ; sol forcé vert = aucun overlay ; donc pixels sol.
      **Écarté** (tests fiables anim figée) : multi-lumières (count=1 confirmé),
      cookies (cookieIdx=-1), ombres colorées (csf scalaire), z-fighting feet/sol
      (sol abaissé → persiste), IBL/SSAO/bloom/FXAA/VFX/post, format buffer (RGBA16F).
      **RenderDoc (2026-06-26, capture skin_frame1051.rdc + API Python)** :
      dump du buffer HDR pré-tonemap (Texture 3248, RGBA16F) en PNG → l'irisation
      EST DÉJÀ DANS LE HDR, sur le sol, près des pieds colorés. Donc c'est la
      **passe Geometry** (le HDR du sol), PAS le post-process. (Piège : le HDR est
      stocké Y-flip → mes PickPixel(620,600) lisaient le HAUT (fond gris), d'où la
      fausse piste "tonemap". Le dump-image lève l'ambiguïté.)
      Confirme la bisection shader initiale (terme Lo du sol). La couleur MATCHE le
      modèle (vert/bleu) → le sol récupère la couleur du modèle alors que la math
      (1 soleil blanc, ombre scalaire, F0 gris) ne le permet pas, SANS réflexion
      (SSR/planar off) ni multi-lumière (count=1). **PROCHAINE ÉTAPE** : suspect =
      passe **VFX** (rend après Geometry dans le HDR) ou bleed géométrique modèle→sol ;
      RenderDoc : dumper le HDR AVANT vs APRÈS la passe VFX au pixel sol.
      Outillage RenderDoc prêt (rdc/rdccap.ps1 capture F12 + analyze*.py API Python).

      **DÉSACTIVATION 1-À-1 (méthode Rihen) + RenderDoc approfondi (2026-06-26)** :
      - **NK_NOMODEL** (sol seul, sans modèle) → sol PARFAITEMENT PROPRE. Donc la
        lueur REQUIERT le modèle.
      - **castShadow=false** (modèle sans ombre) → lueur PERSISTE. Pas l'ombre.
      - **NK_NO_VFX** → persiste. Pas VFX.
      - **Skin frag forcé MAGENTA** (modèle magenta vif) → lueur du sol reste
        VERTE/jaune (pas magenta) ! Donc la lueur n'est PAS la couleur rendue par
        le skin shader — elle a les couleurs de la TEXTURE du modèle (gltf_image
        bleu/vert), fixes.
      - RenderDoc dump EnvPrefilter (256²) → GRIS uniforme, pas de modèle. IBL écarté.
      - DemoSkin n'enregistre AUCUNE réflexion planaire (log : seuls les vieux runs
        demo5 le font). Réflexion planaire écartée.
      - RenderDoc bindings du draw SOL (eid 222, idx=6) : tAlbedo=White1x1, IBL=gris
        → inputs CORRECTS → le shader sol DEVRAIT sortir gris. Modèle (eid 344,
        idx=14016) : tAlbedo=gltf_image.
      **PARADOXE** : le sol a des inputs corrects (albédo blanc) → gris attendu,
      mais une lueur verte (= couleurs de la texture modèle) apparaît, requiert le
      modèle, indépendante du skin shader. Mécanisme final NON identifié malgré
      bisection exhaustive + inspection RenderDoc des bindings/textures.
      **PISTES restantes** : (a) la lueur = jambes/pieds colorés du modèle eux-mêmes
      près du sol (mal interprétés comme "sol") — MAIS magenta réfute (resterait
      magenta) ; (b) bleed sub-pixel/MIP de la texture modèle au bord silhouette ;
      (c) recapturer le HDR AVEC modèle magenta : si HDR sol vert → géométrie pure.
      NK_FREEZE + NK_NOMODEL laissés en place (outils de repro/diag).

## A3 · Features avancées — DIFFÉRÉ (après éditeurs)
GTAO complet · Voxel AO précision GPU · VSM v2 page-based · DOF · Motion blur ·
Matériaux/lumières/ombres 2D.

## Déjà fait cette session (2026-06-26, hors plan)
- 7 loaders maillage from-scratch (OBJ/STL/PLY/FBX bin+ascii/DAE/USDA).
- Fix SSAO-off (fallback `hdr*=hdr.r` → texture blanche).
- Fix double-gamma `pbr.frag.nksl` (retrait `pow(2.2)` sur baseColor sRGB).

## Phase B (autre agent) — Editor Kit
Inspecteur générique (NKReflection ✅) · shell dockable · node-graph/Blueprint ·
UI builder. Je peux débloquer côté NKReflection si besoin (SetObjectArray,
parcours entité NkComponentMask) — sur demande.
