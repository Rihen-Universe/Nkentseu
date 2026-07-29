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
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }
}
