// =========================================================================
// TEMOIN — les noms de macros ADDITIFS repondent vrai, sans que l'ancien
//          nom ait disparu.
// =========================================================================
//
// Arbitrage de Rodolf (2026-08-17) :
//     « WEB et EMSCRIPT sont permis. ARM32, ARM64 et ARM aussi. »
//
// Ce ne sont pas des renommages. Les deux ecritures doivent etre correctes
// SIMULTANEMENT sur la meme cible. Un temoin qui ne verifierait que le nom
// ajoute ne dirait rien du nom historique, donc ne prouverait pas
// l'additivite : il passerait aussi bien sur un renommage, qui casse tout le
// code employant l'ancien nom.
//
// CE QUE CE TEMOIN VERIFIE
//   1. sur cible ARM 32 bits  : NKENTSEU_ARCH_ARM   ET NKENTSEU_ARCH_ARM32
//   2. sur cible Emscripten   : ..._PLATFORM_EMSCRIPTEN ET ..._PLATFORM_WEB
//   3. hors ARM 32 bits       : NKENTSEU_ARCH_ARM32 ne doit PAS apparaitre
//   4. hors Emscripten        : NKENTSEU_PLATFORM_WEB ne doit PAS apparaitre
//
//   Les points 3 et 4 sont ce qui distingue « definir aussi » de « definir
//   partout » : une addition mal placee elargirait la portee de la garde au
//   lieu de la nommer autrement.
//
// CE QUE CE TEMOIN NE VERIFIE PAS
//   - il ne s'execute pas : c'est une verification de PREPROCESSEUR, prise a
//     la compilation croisee. Aucun materiel ARM 32 bits n'a ete sollicite.
//   - il reprend, pour decider ce qu'il attend, les macros predefinies du
//     compilateur (__arm__, __EMSCRIPTEN__) — les memes que celles sur
//     lesquelles l'en-tete declenche. Si le declencheur lui-meme etait faux,
//     ce temoin serait aveugle. Il ne juge que l'additivite, pas la detection.
//
// Rejeu obligatoire sur l'etat d'AVANT : sur l'etat d'avant le correctif, les
// points 1 et 2 doivent ECHOUER a la compilation. Un temoin qui passe des deux
// cotes du correctif ne discrimine rien.
//
// Lancement : bash Kernel/Foundation/NKPlatform/tests/run_temoin_noms_additifs.sh
// =========================================================================

#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkPlatformDetect.h"

// -------------------------------------------------------------------------
// 1. ARM 32 bits — les DEUX noms
// -------------------------------------------------------------------------
#if defined(__arm__) || defined(_M_ARM) || defined(__ARM__) || defined(__arm)

#if !defined(NKENTSEU_ARCH_ARM)
#error "TEMOIN ECHEC : NKENTSEU_ARCH_ARM absent sur cible ARM 32 bits — le nom HISTORIQUE a ete perdu (renommage au lieu d'addition)."
#endif

#if !defined(NKENTSEU_ARCH_ARM32)
#error "TEMOIN ECHEC : NKENTSEU_ARCH_ARM32 absent sur cible ARM 32 bits — le nom AJOUTE manque. NkPlatform.cpp rendra NK_UNKNOWN."
#endif

#endif

// -------------------------------------------------------------------------
// 2. Emscripten / Web — les DEUX noms
// -------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__)

#if !defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#error "TEMOIN ECHEC : NKENTSEU_PLATFORM_EMSCRIPTEN absent sur cible Emscripten — le nom HISTORIQUE a ete perdu."
#endif

#if !defined(NKENTSEU_PLATFORM_WEB)
#error "TEMOIN ECHEC : NKENTSEU_PLATFORM_WEB absent sur cible Emscripten — le nom AJOUTE manque."
#endif

#endif

// -------------------------------------------------------------------------
// 3. Hors ARM 32 bits — pas de sur-definition
// -------------------------------------------------------------------------
#if !defined(__arm__) && !defined(_M_ARM) && !defined(__ARM__) && !defined(__arm)

#if defined(NKENTSEU_ARCH_ARM32)
#error "TEMOIN ECHEC : NKENTSEU_ARCH_ARM32 defini HORS cible ARM 32 bits — l'addition a elargi la garde au lieu de la nommer."
#endif

#endif

// -------------------------------------------------------------------------
// 4. Hors Emscripten — pas de sur-definition
// -------------------------------------------------------------------------
#if !defined(__EMSCRIPTEN__)

#if defined(NKENTSEU_PLATFORM_WEB)
#error "TEMOIN ECHEC : NKENTSEU_PLATFORM_WEB defini HORS cible Emscripten — l'addition a elargi la garde au lieu de la nommer."
#endif

#endif

// Unite de traduction non vide : evite qu'un fichier reduit a des directives
// soit accepte sans que le compilateur ait rien eu a analyser.
extern "C" int nkentseu_temoin_noms_additifs(void);
int nkentseu_temoin_noms_additifs(void) { return 0; }
