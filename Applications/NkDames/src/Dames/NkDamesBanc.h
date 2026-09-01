// =============================================================================
// NkDamesBanc.h — le banc des regles
//
// A QUOI SERT CE FICHIER
//   Il expose UNE fonction : lancer le banc et rendre un verdict. Le code de
//   sortie EST le verdict (0 = vert), ce qui le rend utilisable par un script
//   d'integration continue sans rien lire.
//
// POURQUOI IL EST SEPARE DU JEU
//   Parce qu'il ne depend QUE des regles : ni fenetre, ni GPU, ni image. Il
//   tourne donc sur une machine sans ecran, et son verdict ne depend pas de ce
//   qu'on croit voir.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			/// Rend 0 si tous les cas passent, 1 sinon.
			int32 NkDamesLancerBanc();

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
