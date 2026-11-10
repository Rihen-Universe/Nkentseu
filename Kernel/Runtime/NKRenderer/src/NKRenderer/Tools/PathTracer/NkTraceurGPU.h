#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkTraceurGPU.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LE TRACEUR DE CHEMINS SUR LA CARTE GRAPHIQUE (10/11/2026, rendu-realiste)
// =============================================================================
//  Rodolf (09/11) : « pourquoi le notre n'est pas calcule sur la carte graphique ?
//  on doit pouvoir choisir si on fait le rendu sur la carte graphique ou le
//  processeur. » Ce module rend la MEME image que NkTraceurChemins (processeur) :
//  la meme scene preparee (NkTcEmpaqueter : le meme BVH, les memes matieres, les
//  memes cartes, les memes sources), le meme algorithme ecrit en nuanceur de calcul
//  NkSL (chemin, NEE soleil + RIS, GGX, verre, roulette russe, ecretage), et la
//  meme fin (NkTcFinaliserSommes : le meme debruiteur, la meme exposition).
//  Seul le tirage aleatoire differe (PCG 32 bits par echantillon) : les deux images
//  se rejoignent a convergence. Trois tampons : la scene (vec4), les cartes (uint),
//  la sortie (vec4) -- DX11 n'a que huit vues UAV.
//  Le lancer de rayons materiel (DXR, Vulkan RT) viendra quand le dorsal l'offrira.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"

namespace nkentseu {
	class NkIDevice;
	namespace renderer {

		/// Rend une vue sur la carte graphique (la scene doit etre preparee par NkTcPreparer).
		/// `mutation` : CONTRE-EPREUVE du banc (le nuanceur ne teste plus les ombres). Rend faux et
		/// ecrit pourquoi si le device n'a pas de calcul ou si le nuanceur est refuse.
		bool NkTcRendreGPU(NkIDevice *device, const NkTcCamera &cam, const NkTcReglages &r, const char *png, NkTcStats *stats,
						   bool mutation = false);

	} // namespace renderer
} // namespace nkentseu
