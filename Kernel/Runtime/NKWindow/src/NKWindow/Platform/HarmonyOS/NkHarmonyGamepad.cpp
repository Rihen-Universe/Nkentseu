// =============================================================================
// NkHarmonyGamepad.cpp — backend gamepad HarmonyOS (STUB)
//
// L'implémentation STUB est entièrement inline dans NkHarmonyGamepad.h (toutes
// les méthodes virtuelles sont no-op et retournent des valeurs par défaut),
// donc ce fichier n'a aucun code à fournir. Il est conservé pour rester listé
// dans NKWindow.jenga (filtre system:HarmonyOS) ; le linker ne tire rien d'ici.
//
// Future intégration OH_Input (API 13+) :
//   - OH_Input_GetGamepadAxisValue pour Poll() / GetSnapshot()
//   - OH_Input_SetGamepadVibration pour Rumble()
//   - Détection connect/disconnect via OH_Input_RegisterDeviceListener
// À écrire quand un dispositif HarmonyOS sera disponible pour test.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)
#include "NKWindow/Platform/HarmonyOS/NkHarmonyGamepad.h"
#endif // NKENTSEU_PLATFORM_HARMONYOS
