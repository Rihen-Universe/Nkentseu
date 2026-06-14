// =============================================================================
// NkHarmonyEventSystem.cpp — HarmonyOS event system (STUB)
//
// Implémentation STUB : pour l'instant, le build NKWindow HarmonyOS doit
// passer sans device de test physique disponible. Les callbacks XComponent
// (touch, mouse, key, surface) reviendront sous forme complète quand un
// device HarmonyOS sera connecté pour test.
//
// L'ancien fichier complet utilisait :
//   - <ace/xcomponent/native_xcomponent_event.h>      (header retiré dans
//     les versions récentes du SDK OHOS — consolidé dans
//     <ace/xcomponent/native_interface_xcomponent.h>)
//   - constantes KEY_BACKSPACE/LEFT/RIGHT/UP/DOWN/END (manquantes du SDK)
//   - types NkTouch{Began,Moved,Ended,Cancelled}Event obsolètes (renommés
//     NkTouch{Begin,Move,End,Cancel}Event)
//   - constantes NK_MOUSE_BUTTON_{LEFT,MIDDLE,RIGHT} obsolètes
//
// La réécriture s'appuiera sur :
//   - OH_NativeXComponent_RegisterCallback (déjà fait dans NkHarmonyWindow)
//   - NkTouchPoint (membres id/clientX/clientY/screenX/screenY/pressure)
//   - NkKey (enum class — chiffres NK_NUMx, lettres NK_x, etc.)
//   - Noms d'événements corrects : NkKeyPressEvent / NkKeyReleaseEvent
//                                  NkMouseButtonPressEvent / ReleaseEvent
//                                  NkMouseMoveEvent
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)
#include "NKWindow/Platform/HarmonyOS/NkHarmonyEventSystem.h"
// Header SDK OHOS gardé pour exposer OH_NativeXComponent (utilisé par
// NkEventSystemData et NkHarmonyOnSurfaceCreated dans NkHarmonyWindow.cpp).
#include <ace/xcomponent/native_interface_xcomponent.h>
#endif // NKENTSEU_PLATFORM_HARMONYOS
