# État d'avancement — chantiers HarmonyOS / Web / Android (session en cours)

> Fichier de reprise anti-coupure. Mis à jour après chaque palier prouvé.
> Base : ecabc216 (MatCap). AUCUN commit effectué (consigne).

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
