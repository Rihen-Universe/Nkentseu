# État d'avancement — chantiers HarmonyOS / Web / Android (session en cours)

> Fichier de reprise anti-coupure. Mis à jour après chaque palier prouvé.
> Base : ecabc216 (MatCap). AUCUN commit effectué (consigne).

## CHANTIER 5 — Demo3D (demo 2) sur Web : 🔄 EN COURS

Objectif : scène 3D complète (sphères PBR + ombres + cubes instanciés) dans Chrome.

Paliers PROUVÉS :
- (a) Shaders embarqués : `--preload-file <abs>/Resources/NKRenderer/Shaders@/Resources/NKRenderer/Shaders`
  ajouté via `emscriptenextraflags` (RendererSandbox.jenga, chemin absolu calculé en Python — emcc
  résout depuis SON cwd). Build Web Debug ✓ : `renderdemo.data` 830 Ko, 455 fichiers ; manifeste JS
  contient l'arborescence `Resources/NKRenderer/Shaders/...`. MEMFS cwd = "/" → les chemins C++
  relatifs matchent sans changement moteur.
- (b) Sélection demo : DÉJÀ câblée de bout en bout — le shell HTML
  (`web/emscripten_fullscreen_nofavicon.html`) transforme `?demo=2` en `Module.arguments =
  ["--demo=2"]` → argv de main() (NkEmscripten.h) → ParseDemo. PREUVE (console Chrome headless) :
  `NkRenderer v5.0 - Demo 2 (3D) - Backend : OpenGL` + shaders lus depuis le preload
  (`CompileVF 'Render2D' vsGlsl=734 fsGlsl=7353`).
- (c) BLOQUEUR DÉCOUVERT et MESURÉ (page de sonde `webgl_probe.html`, Chrome headless SwiftShader) :
  WebGL2 = GLSL ES 3.00 STRICT. Résultats sonde : `#version 300 es` OK ; 310/320 es et `430 core`
  REJETÉS ; `layout(binding=N)` REJETÉ (samplers ET UBO) ; `layout(std140)` sans binding OK ;
  22 samplers statiques OK (MAX_TEXTURE_IMAGE_UNITS=32, UBO bindings=72). Or NOS générateurs
  (NkSL codegen `#version 430 core` + SPIRV-Cross ES `320 es`) émettent tous des bindings
  explicites (contrat du backend GL). Les 18 CompileVF échouaient donc TOUS
  (`'core' : invalid version directive`). Android/MEmu passait car le driver GLES de MEmu
  (translation desktop) tolère le GLSL desktop.
- (d) FIX implémenté (à prouver après rebuild) : shim d'adaptation WebGL2 dans
  `NkOpenglDevice.cpp` (`NkWebGL2AdaptGLSL`, sous NKENTSEU_PLATFORM_EMSCRIPTEN uniquement) :
  en-tête → `300 es` + précisions par défaut (samplers shadow/array SANS défaut en ES),
  `binding`/`set` retirés du texte mais COLLECTÉS (nom → binding), `location` retiré des
  varyings (gardé sur VS in / FS out — matching par NOM, identique dans nos générateurs),
  puis ré-application après glLinkProgram : `glUniformBlockBinding` (UBO) + `glUniform1i`
  (samplers). Vérifié : scène 3D = UBO + samplers uniquement (pas de SSBO/imageLoad/
  textureGather ; InstanceUBO 10 Ko < 16 Ko min WebGL2) → représentable.
- Note perf preuve : wasm DEBUG (O0 + SAFE_HEAP) sous SwiftShader met >5 min à passer
  InitEnvironment (IBL) → prendre la capture sur le build RELEASE.
- (e) Shim PROUVÉ côté compilation shader (build Debug rebuildé, Chrome headless) :
  `CompileVF 'Render2D' 734/7353` et `'Glow2D' 651/652` passent SANS AUCUN
  "Shader compile error" (avant le shim : échec `'core' : invalid version directive` sur
  chaque programme). Deux incompatibilités WebGL2 supplémentaires corrigées au passage
  (découvertes au run Release) :
  1. `glVertexAttribFormat/Binding/BindingDivisor` (modèle attrib-binding ES 3.1) N'EXISTENT
     PAS en WebGL2 → pointeurs glad NULS → "RuntimeError: null function" au 1er pipeline.
     FIX : sous Emscripten, le vertex layout est appliqué AU BIND du buffer via
     `glVertexAttrib(I)Pointer` + `glVertexAttribDivisor` (ES 3.0) — nouvelle fonction
     `NkOpenglWebBindVertexBuffer` (NkOpenglDevice.cpp) appelée par GL_BindVertexBuffer.
  2. WebGL2 fige la "classe" d'un buffer au premier bind (index vs autre) → INVALID_OPERATION
     "buffers bound to non ELEMENT_ARRAY_BUFFER targets..." (nos descs ont type=NK_INDEX mais
     bindFlags=NONE → mauvaise cible). FIX : sous Emscripten, create/write/read/map passent par
     les cibles NEUTRES `GL_COPY_READ/COPY_WRITE_BUFFER` (exemptées) ; la classe est fixée par
     le premier bind réel (draw).
  Note logging : en Release-Web les logs moteur n'apparaissent qu'au flush (batch/exit) —
  l'absence de logs pendant l'init N'EST PAS un crash (les appels GL continuent).

Fichiers modifiés (Web) :
- `Applications/Sandbox/RendererSandbox.jenga` : `_NK_SHADERS_DIR` (abs) + `--preload-file` (filtre Web).
- `Kernel/Runtime/NKRHI/src/NKRHI/Opengl/NkOpenglDevice.cpp` : shim WebGL2 (adaptation GLSL +
  ré-application des bindings par nom dans CreateShader). Zéro impact hors Emscripten.

## CHANTIER 4 — Demo3D (demo 2) sur HarmonyOS : 🔄 EN COURS (code écrit, build/preuve à faire)

- (a) Shaders dans le HAP : `harmonyassets(["../../Resources/NKRenderer/Shaders"])`
  (RendererSandbox.jenga, filtre HarmonyOS — l'API jenga existait déjà : copie dans
  entry/src/main/resources/rawfile/) + lien `rawfile.z` (librawfile.z.so, présente dans le
  sysroot OHOS x86_64 — vérifié).
- (b) Repli lecteur rawfile dans NkFile (MÊME MOTIF que l'AAssetManager Android) :
  `NkFile::SetHarmonyResourceManager/GetHarmonyResourceManager` (NkFile.h) + branches
  HarmonyOS dans Open/Close/Read/Tell/Seek/GetSize/IsEOF/Exists (NkFile.cpp,
  OH_ResourceManager_*RawFile64) + `TryOpenHarmonyRawFile` (mêmes variantes de chemin :
  brut, strip "Resources/<SubFolder>/", strip "Resources/" — sous-dossier PARTAGÉ avec
  Android via SetAndroidAssetSubFolder).
- (c) Pont ResourceManager ArkTS → natif : hook FAIBLE `NkHarmonyOnNapiInitExtra(env, exports)`
  appelé par NkHarmonyNapiInit (NkHarmonyOS.h, avant la garde anti double-init) ; renderdemo
  (main.cpp) le définit et exporte `nkSetResMgr` (OH_ResourceManager_InitNativeResourceManager →
  NkFile). Index.ets : `.onLoad((context) => (context as ESObject).nkSetResMgr(
  getContext(this).resourceManager))`. nkmain attend le resMgr (borné 10 s, log).
- (d) Sélection demo sans réinstaller (équivalent du setprop Android) : fichier
  `nk_demo.txt` lu au démarrage dans le sandbox de l'app
  (`/data/storage/el2/base/haps/entry/files/` puis `/data/storage/el2/base/files/`),
  poussé côté hôte via `hdc shell "echo 2 > /data/app/el2/100/base/com.nkentseu.sandbox.render.demo/haps/entry/files/nk_demo.txt"`.
- RESTE : build Debug+Release HarmonyOS, purge `harmony-build/entry/build` avant repackage
  (cache hvigor !), émulateur (UN SEUL à la fois), install, push nk_demo.txt=2, preuve
  snapshot_display scène 3D.

Fichiers modifiés (HarmonyOS, en plus du .jenga ci-dessus) :
- `Kernel/System/NKFileSystem/src/NKFileSystem/NkFile.{h,cpp}` : repli rawfile.
- `Kernel/Runtime/NKWindow/src/NKWindow/EntryPoints/NkHarmonyOS.h` : hook faible NAPI.
- `Applications/Sandbox/src/Demo/main.cpp` : nkSetResMgr + attente resMgr + nk_demo.txt +
  SetAndroidAssetSubFolder (bloc HarmonyOS).
- `Applications/Sandbox/harmony/ets/Index.ets` : onLoad → nkSetResMgr.

## CHANTIER 3 — Assets Android : ✅ TERMINÉ (prouvé)

Palier atteint : scène 3D NON VIDE sur MEmu, 18 programmes shaders lus depuis l'APK.

Preuves :
- APK contient 455 fichiers `assets/` (shaders) : `unzip -l renderdemo-Debug.apk | grep -c assets/` → 455.
- logcat (MEmu, 127.0.0.1:21503) : 18 lignes `CompileVF 'X' vsGlsl=N fsGlsl=M` toutes > 0
  (PBR vsGlsl=2339 fsGlsl=22020, Skin 2682/5147, Render2D 734/7353, ... ; `grep -c "vsGlsl=0 "` → 0).
  Les shaders passent par le "chemin NkSL (vrai dialecte)" → fichiers .nksl lus depuis assets/.
- Capture non vide : `Captures/nk_android_demo3d.png` (demo 3D via `setprop debug.nk.demo 2`,
  sphères PBR + ombres + grille cubes, HUD "Demo 3D | API : OpenGL", 59 FPS).
  Capture demo 0 : `Captures/nk_android_assets.png` (HUD subsystems).

Fichiers modifiés :
- `Applications/Sandbox/RendererSandbox.jenga` : `androidassets(["../../Resources/NKRenderer/Shaders"])`
  dans le filtre Android de sandboxusenkrenderer (+ import harmonyets, cf. chantier 1).
- `Applications/Sandbox/src/Demo/main.cpp` : `NkFile::SetAndroidAssetSubFolder("NKRenderer/Shaders")`
  au début de nkmain (+ include NKFileSystem/NkFile.h).
- `Kernel/System/NKFileSystem/src/NKFileSystem/NkFile.cpp` : repli AAssetManager ajouté à `NkFile::Exists`
  (sinon les overrides .vk.glsl/.nksl ne sont jamais détectés sur Android).
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Shader/NkShaderLibrary.cpp` : 3 lectures fopen brutes
  (ReadFileToString, NkShaderLibrary::ReadFile, lambda readRaw) basculées sur `NkFile::ReadAllText`.
- `Kernel/Runtime/NKRenderer/src/NKRenderer/Shader/NkShaderIncludeResolver.cpp` : `ReadRaw` → `NkFile::ReadAllText`.

## CHANTIER 2 — Web (contexte WebGL) : ✅ TERMINÉ (prouvé)

Palier atteint : contexte WebGL2 créé + GLAD chargé + RHI initialisé + render graph exécuté dans Chrome headless.

Preuves (console Chrome headless SwiftShader, `python -m http.server 8123` sur Build/Bin/Debug-Web/renderdemo,
log complet : /tmp/chrome_console.log de la session) :
- `[NkRHI_GL] WebGL2 context OK (canvas '#canvas')`
- `[NkRHI_GL] Initialized (GL 3.0, WebKit WebGL)` + `ES caps: GLSL=OpenGL ES GLSL ES 3.00 (WebGL ...)`
- `[NkDeviceFactory] Device RHI cree: OpenGL` → `[NkRendererImpl] Initialize done` →
  `[NkRenderGraph] Execute frame=1 : passes=1 swap=1280x720` → `BeginRenderPass fbo=0 ... clear=(0.05,0.05,0.07,1)`.
- Capture `Captures/nk_web_headless.png` : canvas rempli avec LA couleur de clear du moteur (0.05,0.05,0.07)
  — le contexte présente bien à l'écran (avant : crash immédiat, GLAD nul).
- Notes bénignes : INVALID_ENUM sur quelques caps queries GL desktop non supportées par WebGL (non bloquant) ;
  `pipeValid=0` sur les batches 2D à frame 1 (screenshot pris trop tôt, virtual-time-budget).

Fichiers modifiés :
- `Kernel/Runtime/NKRHI/src/NKRHI/Opengl/NkOpenglDevice.cpp` :
  - includes : bloc `NKENTSEU_PLATFORM_EMSCRIPTEN` (<emscripten/html5.h> + extern gladLoadGLES2) ;
  - `Initialize` : branche `#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)` — emscripten_webgl_create_context
    (WebGL2 obligatoire, canvas `init.surface.canvasId` défaut "#canvas") + make_context_current +
    gladLoadGLES2(emscripten_webgl_get_proc_address). Motif repris de NkContext.cpp (~l.1024).
  - `Shutdown` : destruction du contexte WebGL ;
  - `SubmitAndPresent` : branche Web = glFlush (composition implicite par le navigateur).
- `Kernel/Runtime/NKRHI/src/NKRHI/Opengl/NkOpenglDevice.h` : membre `long mWebGLContext` (Emscripten).

## CHANTIER 1 — HarmonyOS : ✅ PALIER (d) ATTEINT — image non noire rendue par le moteur

PREUVE FINALE (émulateur relancé après redémarrage, hdc tconn 127.0.0.1:5555) :
- Plus AUCUNE erreur de relocation au chargement NAPI (hilog propre après ajout ace_ndk.z + ace_napi.z).
- hilog : le thread natif (tid distinct du main) exécute des appels GL via la couche DGLES de l'émulateur
  (d_glDisable/d_glGetError — INVALID_ENUM bénins sur enums desktop, comme sur Web) → nkmain + RHI actifs.
- Capture `Captures/nk_harmony_renderdemo.jpeg` (snapshot_display) : rendu du MOTEUR à l'écran —
  HUD "Draw:0 Tris:0 GPU:0.00ms CPU:0.00ms Batches:0" (haut) + "Active: R2D|R3D|TEXT|OVERLAY" (bas),
  fond de clear vert — IDENTIQUE au HUD renderdemo (demo 0 Subsystems) vu sur MEmu. Plus de "Hello World".
- Ajouts finaux au link (RendererSandbox.jenga, filtre HarmonyOS) :
  `_HARMONY_LINKS = ["hilog_ndk.z", "EGL", "GLESv3", "ace_ndk.z", "ace_napi.z", "native_window"]`
  (ace_ndk.z = OH_NativeXComponent_*, ace_napi.z = napi_module_register, native_window = OHNativeWindow).

### (historique de la remontée, avant la preuve finale)

Paliers atteints :
- (a) HAP construit avec page XComponent + .so chargeable : le XComponent ArkTS tente bien le chargement
  (`hilog : LoadNativeModule key is default/renderdemo` + `XComponent[nk_surface] triggers onLoad and
  OnSurfaceCreated callback`).
- Cascade de symboles manquants résolue, prouvée par llvm-nm sur la .so DANS le HAP final :
  1. libc++_shared.so absente du HAP → copiée depuis le NDK OHOS dans entry/libs/x86_64/ (voir "reste à faire") ;
  2. `glslang::InitGlobalLock` etc. → NKGLSlang.jenga n'avait AUCUN filtre HarmonyOS (ossource.cpp jamais
     compilé) → filtre ajouté (Unix/ossource.cpp, ohos-ndk, -fPIC) ;
  3. `NkEventSystem::{Init,PumpOS,Enqueue_Public}` → NkHarmonyEventSystem.cpp était un stub vide → implémentation
     minimale ajoutée (PumpOS no-op : événements poussés par callbacks) + garde HarmonyOS sur le Shutdown commun.
  Vérif finale : `llvm-nm -D --undefined-only <so du HAP> | grep -c nkentseu` → 0. HAP re-packagé (purge cache
  hvigor nécessaire : `rm -rf harmony-build/entry/build` sinon CacheNativeLibs re-packageait l'ancienne .so).
- Install du HAP final : PAS ENCORE relancé (émulateur HarmonyOS absent après redémarrage PC).

Fichiers modifiés/créés :
- `Kernel/Runtime/NKWindow/src/NKWindow/EntryPoints/NkHarmonyOS.h` : récupération OH_NATIVE_XCOMPONENT_OBJ
  depuis exports (napi_unwrap) + OH_NativeXComponent_RegisterCallback → NkHarmonyOnSurface* ; attente de la
  surface (20 s max) avant nkmain (équivalent APP_CMD_INIT_WINDOW Android) ; garde anti double-init.
- `Kernel/Runtime/NKWindow/src/NKWindow/Platform/HarmonyOS/NkHarmonyWindow.{h,cpp}` : surface XComponent
  "en attente" (arrivée avant NkWindow::Create) adoptée par Create ; NkHarmonySurfaceReady() ; repli
  d'adoption dans OnSurfaceCreated (mXComponentId jamais renseigné avant).
- `Kernel/Runtime/NKWindow/src/NKWindow/Platform/HarmonyOS/NkHarmonyEventSystem.cpp` : impl minimale
  Init/Shutdown/PumpOS/GetPlatformName/Enqueue_Public.
- `Kernel/Runtime/NKEvent/src/NKEvent/NkEventSystem.cpp` : garde `!NKENTSEU_PLATFORM_HARMONYOS` sur le
  Shutdown commun (doublon de link sinon).
- `Applications/Sandbox/src/Demo/main.cpp` : `NKENTSEU_HARMONY_DEFINE_MODULE(renderdemo)` en fin de fichier
  (le module NAPI n'était JAMAIS défini pour renderdemo → rien ne démarrait).
- `Applications/Sandbox/harmony/ets/Index.ets` (NOUVEAU) : page ArkTS avec
  `XComponent({id:'nk_surface', type:XComponentType.SURFACE, libraryname:'renderdemo'})`.
- `Applications/Sandbox/RendererSandbox.jenga` : `harmonyets(["harmony/ets/Index.ets"])` dans le filtre
  HarmonyOS (+ `from Jenga.Core.Api import harmonyets` — non exporté par `from Jenga import *`).
- `Externals/Libs/NKGLSlang/NKGLSlang.jenga` (SOUS-MODULE) : filtre system:HarmonyOS.

Reste à faire (HarmonyOS) :
- libc++_shared.so : PÉRENNISÉ dans Jenga (hors repo, source editable utilisée par jenga.exe) :
  `D:\Projets\MacShared\Projets\Jenga\Jenga\Core\Builders\HarmonyOs.py` — nouvelle méthode
  `_CopyOhosSharedStl(libs_dir, abi)` appelée après la copie des .so (les 2 branches ABI) : copie
  `<sdk>/native/llvm/lib/<triple>/libc++_shared.so` dans entry/libs/<abi>/ (mapping abi→triple).
- Relancer l'émulateur (`C:\ohos\Emulator\HarmonyOS-NEXT-miku404\tools\emulator\Emulator.exe`, port hdc 5555,
  `hdc tconn 127.0.0.1:5555`), réinstaller le HAP final, prouver nkmain/hilog + snapshot.

## Environnement / outils validés
- hdc : `hdc tconn 127.0.0.1:5555` obligatoire (list targets = [Empty] sinon). Port 5555 = process Emulator.
- adb MEmu : `"D:\Program Files\Microvirt\MEmu\adb.exe" -s 127.0.0.1:21503`.
- Source Jenga vivante : `D:\Projets\MacShared\Projets\Jenga\Jenga` (2.0.9) — utilisée par jenga.exe.

## Statuts de build (renderdemo)
- Windows Debug : ✓ SUCCESS 29/29 (après tous les edits).
- Windows Release : ✓ SUCCESS 29/29.
- Web Debug : ✓ SUCCESS (rebuild post-chantier 3) ; Web Release : ✓ SUCCESS (renderdemo.wasm généré).
- Android Debug : ✓ SUCCESS + APK universal 4 ABIs (455 assets shaders) — exécution prouvée sur MEmu.
- HarmonyOS Debug : ✓ SUCCESS + HAP — exécution prouvée sur émulateur (capture jpeg).
- Android Release : ✓ SUCCESS — Universal APK 4 ABIs (`Release-Android/renderdemo/android-build-universal/renderdemo-Release.apk`).
- HarmonyOS Release : ✓ SUCCESS — HAP généré (`Release-HarmonyOS/renderdemo/renderdemo.hap`) avec copie
  AUTOMATIQUE de libc++_shared.so par la modif Jenga (`Copied: libc++_shared.so (NDK OHOS)` dans le log).
