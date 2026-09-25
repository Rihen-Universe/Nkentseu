#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/PV3DE/src/PV3DE/Face/NkFaceController.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   PV3DE CONSOMME le calcul facial, il ne le POSSEDE plus.
//
// CE FICHIER NE CONTIENT PLUS DE CODE (Rodolf, 25/09/2026)
//   « le facial, que ce soit rig ou deformation ou calcul, je pense que c'est
//     necessaire a tout ce qui demande des personnages, des animaux, etc. qui
//     ont des faces. Donc ca doit vivre ou tous peuvent en profiter. »
//
//   Le calcul est descendu dans
//   Kernel/Runtime/NKAnima/src/NKAnima/Face/NkFaceController.{h,cpp}
//
//   Une APPLICATION est une FEUILLE du graphe de dependances : tant que le
//   calcul vivait ici, AUCUN autre programme ne pouvait en dependre. Ce n'etait
//   pas un cablage manquant, c'etait l'ADRESSE qui l'interdisait -- et aucun
//   branchement n'aurait pu la contourner.
//
// ⚠️ LE PONT PORTE AUSSI LES ACTION UNITS. L'en-tete d'origine incluait
//    NkActionUnit.h ; un fichier qui n'inclurait que celui-ci obtiendrait le
//    controleur mais pas les identifiants d'AU -- et NkSpeechEngine.cpp, qui
//    appelle SetAUTarget avec un NkActionUnitId, ne compilerait plus.
//    *Un pont doit porter TOUT ce que portait ce qu'il remplace.*
//
//    CONDITION DE RETRAIT : le jour ou les appelants de PV3DE nomment
//    anim::NkFaceController directement, ce fichier part.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NkActionUnit.h"
#include "NKAnima/Face/NkFaceController.h"
