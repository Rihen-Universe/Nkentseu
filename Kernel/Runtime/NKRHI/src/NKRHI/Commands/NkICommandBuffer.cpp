// =============================================================================
// NkICommandBuffer.cpp
//
// Une seule chose ici : le CORPS PAR DEFAUT de `ClearBuffer`.
//
// ⚠️ POURQUOI CE FICHIER EXISTE (2026-08-16).
// `NkICommandBuffer::ClearBuffer` etait declare avec un corps VIDE et AUCUN des
// six backends (Vulkan, DirectX11, DirectX12, OpenGL, Metal, Software) ne le
// surchargeait. Tout appel etait donc un no-op *silencieux*, sur toutes les
// plateformes. Deux appelants comptaient dessus — `NKRHI/Core/NkML.cpp:77`, qui
// porte meme le commentaire « Init gradient a zero », et `:106` : ces tampons
// n'etaient mis a zero nulle part.
//
// Le motif est plus general que ce bug : un virtuel a corps vide ne se distingue
// pas d'un virtuel implemente, ni au point d'appel, ni a la lecture, ni au
// commentaire. Le seul moyen de le distinguer est qu'il PARLE. C'est la meme
// regle que « un parametre qui n'est pas honore est pire qu'un parametre absent :
// l'absence force a chercher, la presence dispense de verifier ».
//
// Le corps est defini ICI plutot que dans l'en-tete pour ne pas tirer NKLogger
// dans `NkICommandBuffer.h`, inclus par une trentaine d'applications.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {

	void NkICommandBuffer::ClearBuffer(NkBufferHandle buffer, uint32 value, uint64 offset, uint64 size) {
		(void)buffer;
		(void)value;
		(void)offset;
		(void)size;

		// Compteur de diagnostic. Pas d'atomique : une course benigne fausserait au
		// pire le rang d'une ligne de journal, jamais un resultat de calcul.
		static uint64 sAppels = 0;
		++sAppels;

		// Les 8 premiers appels, puis un sur 10 000 : assez pour que ca se voie des
		// le premier pas, pas assez pour noyer un journal d'entrainement (le chemin
		// NKAI en emettrait 2 700 par pas).
		if (sAppels <= 8 || (sAppels % 10000) == 0) {
			logger.Warnf("[NkRHI] ClearBuffer NON IMPLEMENTE sur ce backend : l'appel ne met RIEN a zero "
						 "(appel #%llu). Seul le backend Vulkan le surcharge (vkCmdFillBuffer). Un appelant "
						 "qui a besoin de la garantie doit la VERIFIER par un temoin ecriture/relecture, pas "
						 "se fier a la presence de la signature.",
						 (unsigned long long)sAppels);
		}
	}

} // namespace nkentseu
