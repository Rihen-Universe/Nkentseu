/**
 * Index.ets — page ArkTS de renderdemo (HarmonyOS).
 *
 * Remplace le "Hello World" du template DevEco : héberge un XComponent de
 * type SURFACE dont `libraryname: 'renderdemo'` charge librenderdemo.so.
 * Au chargement, le runtime appelle NkHarmonyNapiInit() (NkHarmonyOS.h) qui :
 *   1. enregistre les callbacks de surface (OnSurfaceCreated → NkHarmonyWindow) ;
 *   2. lance nkmain() dans un thread dédié (attend la surface avant de démarrer).
 *
 * Fichier copié dans entry/src/main/ets/pages/ par Jenga (harmonyets()).
 */

import resourceManager from '@ohos.resourceManager';

// Surface typee des exports NAPI de librenderdemo.so (cf. NkHarmonyOnNapiInitExtra
// dans main.cpp) — l'ArkTS strict interdit any/unknown/ESObject (arkts-no-any-unknown).
interface NkNativeExports {
  nkSetResMgr?: (mgr: resourceManager.ResourceManager) => void;
}

@Entry
@Component
struct Index {
  build() {
    Stack() {
      XComponent({
        id: 'nk_surface',
        type: XComponentType.SURFACE,
        libraryname: 'renderdemo'
      })
        .onLoad((context) => {
          // `context` = objet exports du module natif (librenderdemo.so).
          // On transmet le ResourceManager de l'app au natif pour que NkFile
          // puisse lire les shaders packages dans resources/rawfile/ du HAP
          // (repli rawfile — equivalent AAssetManager Android).
          const nk = context as NkNativeExports;
          if (nk.nkSetResMgr !== undefined) {
            nk.nkSetResMgr(getContext(this).resourceManager);
          }
        })
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }
}
