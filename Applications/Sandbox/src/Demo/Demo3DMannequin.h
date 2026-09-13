#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Demo3DMannequin.h — SONDE « vêtements sur un mannequin en mouvement »
// (NK_MANNEQUIN_PROBE=1, 2026-09-05). Répond par la mesure à Rodolf : « simuler
// des vêtements sur des mannequins en mouvement sans que le tissu n'entre dans
// les mesh du mannequin ».
//
// Ce que la sonde fait, chaque image (pas FIXE 1/60) :
//   1. évalue le squelette du modèle skinné (glTF .glb ou FBX) sur son clip de
//      marche, IN PLACE : la translation horizontale de la racine du clip est
//      VERROUILLÉE (mesurée d'abord : première contre dernière image), et c'est
//      la sonde qui pousse le bassin dans le monde (NK_MANNEQUIN_SPEED m/s en
//      ligne droite ; le jeu le fera par ses entrées, jamais par une touche
//      injectée) -- décision de Rodolf, DECISIONS bloc « vêtements » ;
//   2. skinne la peau sur le CPU (positions ET normales) : c'est ce maillage-là
//      qui est dessiné ET qui sert au test point-dans-maillage -- le corps
//      testé est le corps vu ;
//   3. pose les capsules du mannequin (NkMannequin, ajustées sur la peau au
//      repos) dans cloth.colliders de chaque vêtement (même ordre : le tissu
//      interpole entre deux poses), met les épingles à cible, fait un pas ;
//   4. mesure : pénétration max des centres dans les capsules, particules À
//      L'INTÉRIEUR du maillage skinné (parité), erreur des épingles, étirement,
//      coût du pas et du test, en ms ; imprime toutes les 60 images et le
//      MAXIMUM sur la course à la fin (NK_MAXFRAMES).
//
// Variables : NK_MANNEQUIN_MODEL (défaut Resources/Models/CesiumMan/CesiumMan.glb),
// NK_MANNEQUIN_ANIM (fichier d'animation à part : mesuré, dit), NK_GARMENTS
// ("cape,jupe,foulard,chapeau" ; aussi tshirt, chemise, robe, pantalon),
// NK_MANNEQUIN_POS ("x,y,z"), NK_MANNEQUIN_SPEED (m/s, défaut 1,2),
// NK_MANNEQUIN_TURN (secondes avant le demi-tour, 0 = jamais), NK_GARMENT_IT /
// NK_GARMENT_SUB (itérations / sous-pas), NK_MANNEQUIN_QUANTILE (rayon des
// capsules), NK_MANNEQUIN_FLIP (retourne les triangles des vêtements).
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKPhysics/NkMannequin.h"
#include "NKPhysics/NkGarment.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

	struct Demo3DMannequinProbe;

	// Crée la sonde si NK_MANNEQUIN_PROBE=1 (nullptr sinon). Charge, mesure, construit.
	Demo3DMannequinProbe *Demo3DMannequinInit(renderer::NkMeshSystem *meshSys);
	// Un pas fixe : squelette, peau, capsules, vêtements, mesures. `frame` pour l'impression périodique.
	void Demo3DMannequinUpdate(Demo3DMannequinProbe *p, renderer::NkMeshSystem *meshSys, uint64 frame);
	// Soumet corps, vêtements, chapeau.
	void Demo3DMannequinDraw(Demo3DMannequinProbe *p, renderer::NkRender3D *r3d, renderer::NkMeshHandle cylinder);
	// Bilan final (maximums sur la course), à la sortie.
	void Demo3DMannequinReport(Demo3DMannequinProbe *p);

} // namespace nkentseu
