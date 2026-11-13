// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkBancAccel.h — (13/11/2026, plan RTX etape 1) le banc des structures d'acceleration du RHI.
// =============================================================================
#pragma once
#include "NKCore/NkTypes.h"

namespace nkentseu {

	/// Le banc des structures d'acceleration (BLAS, TLAS) : un dispositif Vulkan SANS fenetre qui les demande, une BLAS
	/// (un carre de deux triangles) et une TLAS de deux instances (translation, rotation, echelle non uniforme), des
	/// rayons lances par rayQuery dans un nuanceur de calcul, compares a l'intersection exacte calculee au processeur
	/// (instance touchee et distance). Puis le « non pris en charge » : Vulkan sans la demande, et le dorsal logiciel.
	/// Contre-epreuve : NK_RTX_MUTATION=1 (transformation des instances transposee). Renvoie le nombre d'echecs.
	/// (etape 2) Avant : rayQuery DANS NkSL -- le GLSL Vulkan, le SPIR-V 1.4 (capacite RayQueryKHR), le refus clair des
	/// 7 autres cibles, un temoin sans rayQuery qui compile partout ; puis le meme programme NkSL tire sur la carte.
	/// Contre-epreuves : NK_NKSL_RQ_MUTATION=constante (impact « triangle » ecrit « aucun ») et =refus (aucun refus).
	/// NKCraft : NK_REALISTE=banc_rtx.
	int32 NkRhiBancAccel();

} // namespace nkentseu
