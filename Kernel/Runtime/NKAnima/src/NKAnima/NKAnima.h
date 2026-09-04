#pragma once
// =============================================================================
// NKAnima.h — l'en-tête d'AGRÉGATION du module. Le seul fichier de la racine.
//
// LA RACINE EST UN CONTRAT, PAS UNE SALLE D'ATTENTE (règle fixée le 2026-09-04,
// sur la demande de Rodolf « des dossiers spécifiques pour chacun »). Elle porte
// ce qui appartient au module ENTIER, jamais à un domaine : cet en-tête, et au
// plus un en-tête de types réellement partagés par plusieurs domaines, justifié
// en une ligne. Un fichier nouveau qui ne trouve pas son dossier n'atterrit pas
// ici « en attendant » : il CRÉE son dossier, même seul dedans.
//
// Les consommateurs incluent soit ce fichier, soit `NKAnima/<Domaine>/<Fichier>.h`
// — jamais un chemin de racine. C'est le grep qui le prouve.
//
//   Skeleton/  la structure de squelette du moteur (topologie + repos, monde)
//   Clip/      les clips : clés, pistes, échantillonnage, mélange, HFSM
//   Retarget/  rejouer un clip d'un squelette sur un autre
//   Motion/    la couche trajectoire (spline + suivi)
//   Physics/   la physique de POSE : masse, équilibre, appuis, auto-pose (ex-NKAnimPhysics)
//   Edit/      le MODÈLE d'édition de poses-clés — aucune interface
// =============================================================================

#include "NKAnima/Skeleton/NkSkeletonDef.h"
#include "NKAnima/Clip/NkAnimation.h"
#include "NKAnima/Retarget/NkAnimRetarget.h"
#include "NKAnima/Motion/NkMotionPath.h"
#include "NKAnima/Physics/NkPoseMass.h"
#include "NKAnima/Physics/NkBalance.h"
#include "NKAnima/Physics/NkPoseBalancer.h"
#include "NKAnima/Physics/NkContactDetector.h"
#include "NKAnima/Physics/NkAutoPose.h"
#include "NKAnima/Physics/NkClipBalancePass.h"
#include "NKAnima/Edit/NkAnimationEditor.h"
