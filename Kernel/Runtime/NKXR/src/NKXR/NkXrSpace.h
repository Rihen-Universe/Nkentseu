//
// NkXrSpace.h
// =============================================================================
// Description :
//   Les espaces de référence XR et leur sémantique. Un espace n'est pas une
//   donnée, c'est un CONTRAT sur l'origine : toute pose rendue par le runtime
//   est exprimée « dans » l'un d'eux, et l'application choisit lequel.
//
// Caractéristiques :
//   - VIEW  : origine entre les deux yeux, suit la tête. Pour ce qui est
//     solidaire du visage (réticule, HUD de debug).
//   - LOCAL : origine à la pose de tête au moment de Begin(), alignée sur la
//     gravité, yaw figé sur le regard initial. Pour l'expérience assise.
//   - STAGE : origine au SOL au centre de la zone de jeu, +Y vers le haut.
//     Pour le roomscale — c'est le seul espace où « y = 0 » veut dire
//     « le plancher », donc le seul où poser une scène a un sens physique.
//   - À l'étage 0, un espace est identifié par son seul type : le simulateur
//     n'a qu'une zone de jeu. L'objet existe quand même (plutôt qu'un enum nu
//     dans les signatures publiques hautes) pour que l'étage 2 puisse y
//     accrocher le handle runtime sans changer les appelants.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRSPACE_H__
#define __NKENTSEU_XR_NKXRSPACE_H__

#include "NKXR/NkXrTypes.h"
#include "NKXR/NkXrPose.h"

namespace nkentseu {
	namespace xr {

		class NkXrSpace {
			public:
				explicit NkXrSpace(NkXrSpaceType type) noexcept
					: mType(type) {
				}

				NkXrSpaceType GetType() const noexcept {
					return mType;
				}

			private:
				NkXrSpaceType mType;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRSPACE_H__
