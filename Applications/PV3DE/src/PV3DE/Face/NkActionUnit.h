#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/PV3DE/src/PV3DE/Face/NkActionUnit.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   PV3DE CONSOMME les Action Units, il ne les POSSEDE plus.
//
// CE FICHIER NE CONTIENT PLUS DE CODE (Rodolf, 25/09/2026)
//   « le facial, que ce soit rig ou deformation ou calcul [...] doit vivre ou
//     tous peuvent en profiter. »
//
//   Les Action Units du Facial Action Coding System sont descendues dans
//   Kernel/Runtime/NKAnima/src/NKAnima/Face/NkActionUnit.h
//
//   Une APPLICATION est une FEUILLE du graphe de dependances : tant que ce code
//   vivait ici, AUCUN autre programme ne pouvait en dependre -- ni Noge, ni
//   Nogee, ni NKAnee, ni NKScena, ni le modeleur. Ce n'etait pas un cablage
//   manquant, c'etait l'ADRESSE qui l'interdisait.
//
// ⚠️ POURQUOI CE PONT EXISTE, AU LIEU D'UNE SUPPRESSION SECHE. Des dizaines de
//    sites de PV3DE ecrivent NkActionUnitId SANS QUALIFICATIF, depuis
//    namespace pv3de. Les reecrire tous aurait melange deux gestes dans un seul
//    commit : le demenagement, et la reecriture des appelants.
//    CONDITION DE RETRAIT : le jour ou PV3DE nomme anim::NkActionUnitId
//    directement, ce fichier part.
//
// ⚠️ ET IL IMPORTE TOUT L'ESPACE DE NOMS, IL N'ENUMERE PAS. Une premiere version
//    aliasait deux symboles a la main : la construction a echoue sur
//    kAU_PainSevere, kAU_PainMild, kAU_Anxious, kAU_Nauseous, kAU_Exhausted --
//    qu'elle n'avait pas prevus. *Un pont doit porter TOUT ce que portait ce
//    qu'il remplace, et une liste ecrite a la main ne le sait jamais.*
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKAnima/Face/NkActionUnit.h"

namespace nkentseu {
	namespace pv3de {
		using namespace nkentseu::anim;
	} // namespace pv3de
} // namespace nkentseu
