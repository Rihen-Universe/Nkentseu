// -----------------------------------------------------------------------------
// @File    NkGuiEcouteurs.cpp
// @Brief   La table d'ecouteurs posee par l'hote.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ UN POINTEUR, PAS UNE COPIE -- meme choix que les icones (`NkGuiPoserIcones`).
//    La table appartient a l'hote : c'est lui qui la remplit, et il doit pouvoir
//    la modifier apres l'avoir posee. En garder une copie ici donnerait DEUX
//    tables incapables de se contredire -- l'hote brancherait un ecouteur que le
//    monteur ne verrait jamais, sans que rien ne le signale.
// -----------------------------------------------------------------------------

#include "NKGui/Doc/NkGuiEcouteurs.h"

namespace nkentseu {
	namespace nkgui {

		namespace {
			NkGuiEcouteurs *gTable = nullptr;
		}

		NkGuiEcouteurs *NkGuiEcouteursPoses() noexcept {
			return gTable;
		}

		void NkGuiPoserEcouteurs(NkGuiEcouteurs *table) noexcept {
			gTable = table;
		}

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
