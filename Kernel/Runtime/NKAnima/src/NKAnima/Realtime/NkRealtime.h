#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRealtime.h — LA PHYSIQUE D'ANIMATION EN TEMPS REEL (06/10/2026).
//
// Ce que le jeu (Noge), le sequenceur (NKScena) et le simulateur (PV3DE)
// appellent A CHAQUE IMAGE, sans dependre d'aucun moteur physique :
//
//   NkSpringBones   le mouvement SECONDAIRE a ressorts (os suiveurs : raideur,
//                   amortissement, gravite, angle maximal, spheres) ;
//   NkLiveBalance   l'EQUILIBRE : le centre de masse garde au-dessus des appuis ;
//   NkFootIK        l'IK DES PIEDS sur sol irregulier (lancer de rayon de l'hote) ;
//   NkRagdollBlend  la BASCULE animation <-> ragdoll avec fondu (chute, choc),
//                   et le RELEVEMENT ;
//   NkRtCharacter   les quatre, dans le bon ordre, pour un personnage.
//
// Le sol vient de l'hote (NkRtGround) ; le squelette, d'une vue construite une
// fois (NkRtRig). La FABRICATION (corriger toute une animation, l'auto-pose de
// corps entier, cuire le secondaire) n'est PAS ici : elle est dans l'atelier
// prive (Kernel/AnimaAtelier), qui s'appuie sur ces briques.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtRig.h"
#include "NKAnima/Realtime/NkRtGround.h"
#include "NKAnima/Realtime/NkSpringBones.h"
#include "NKAnima/Realtime/NkLiveBalance.h"
#include "NKAnima/Realtime/NkFootIK.h"
#include "NKAnima/Realtime/NkRagdollBlend.h"
#include "NKAnima/Realtime/NkRtCharacter.h"
