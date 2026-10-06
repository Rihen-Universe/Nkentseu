#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
//   Skeleton/  la structure de squelette du moteur (topologie + repos, monde) ; le
//              squelette 2D = ce meme squelette CONTRAINT AU PLAN (NkSkeleton2D, R30)
//   Clip/      les clips : clés, pistes, échantillonnage, mélange, HFSM (vraie
//              depuis le 2026-09-29 : sous-machines, any-state par niveau,
//              déclencheurs, priorités, fichier .nkanimctl), et le
//              REGISTRE qui rend un `clipHandle` résoluble (il désigne, il ne
//              possède pas — les applications gardent leurs clips)
//   Retarget/  rejouer un clip d'un squelette sur un autre
//   Motion/    la couche trajectoire (spline + suivi)
//   Physics/   la physique de POSE : masse, équilibre, appuis, auto-pose (ex-NKAnimPhysics)
//   Edit/      le MODÈLE d'édition de poses-clés — aucune interface
//   Rig/       (02/10) le rig 3D : armature EDITABLE (tete, queue, roulis, symetrie X), rig
//              AUTOMATIQUE (reperes, modeles humanoide / quadrupede / chaine, controles IK),
//              mannequins de test, et le DOCUMENT du rig avec son annulation
//   Skin/      (02/10) la peau : maillage soude, poids automatiques (chaleur, voxels
//              geodesiques), peinture, verification
//   Morph/     (02/10) les formes (shape keys) : base, cibles relatives, pilotes, pistes du clip
//   Realtime/  (06/10) la physique d'animation EN TEMPS REEL, par image : ressorts (secondaire),
//              IK des pieds sur sol irregulier, equilibre, bascule ragdoll et relevement
// =============================================================================

#include "NKAnima/Skeleton/NkSkeletonDef.h"
#include "NKAnima/Skeleton/NkSkeleton2D.h" // (R30, 02/10) le squelette 2D : contraint au plan, emplacements, IK a deux os, .nkskel
#include "NKAnima/Clip/NkAnimation.h"
#include "NKAnima/Clip/NkClipRegistry.h"
#include "NKAnima/Blend/NkAnimMix.h" // (01/10 soir) le melange : poses, masques, arbres, couches, NLA
#include "NKAnima/Retarget/NkAnimRetarget.h"
#include "NKAnima/Motion/NkMotionPath.h"
#include "NKAnima/Physics/NkPoseMass.h"
#include "NKAnima/Physics/NkBalance.h"
#include "NKAnima/Physics/NkPoseBalancer.h"
#include "NKAnima/Physics/NkContactDetector.h"
#include "NKAnima/Physics/NkAutoPose.h"
#include "NKAnima/Physics/NkClipBalancePass.h"
#include "NKAnima/Edit/NkAnimationEditor.h"
#include "NKAnima/Rig/NkArmature.h"   // (02/10) le rig 3D
#include "NKAnima/Rig/NkAutoRig.h"
#include "NKAnima/Rig/NkRigMannequin.h"
#include "NKAnima/Rig/NkRigDocument.h"
#include "NKAnima/Rig/NkRigSuggestions.h" // (05/10) les conseils du rig (l'assistant de l'éditeur)
#include "NKAnima/Skin/NkSkinMesh.h"
#include "NKAnima/Skin/NkSkinWeights.h"
#include "NKAnima/Morph/NkShapeKeys.h"
#include "NKAnima/Realtime/NkRealtime.h" // (06/10) la physique d'animation en temps reel
