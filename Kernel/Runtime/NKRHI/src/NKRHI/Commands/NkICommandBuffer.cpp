// =============================================================================
// NkICommandBuffer.cpp
//
// Les CORPS PAR DEFAUT de `ClearBuffer` et `ClearTexture`.
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

	// =========================================================================
	// ⚠️ `ClearTexture` : LE MEME PIEGE, DESAMORCE AVANT SA PREMIERE VICTIME.
	//
	// Corps vide, 0 surcharge sur les six backends, et **0 appelant** au moment
	// ou l'on ecrit ces lignes (mesure : une seule occurrence dans tout le depot,
	// sa propre declaration). `ClearBuffer` avait exactement la meme forme et a
	// menti pendant des semaines a deux appelants qui le croyaient implemente.
	//
	// La difference tient a un accident de calendrier, pas de conception : on l'a
	// trouve avant que quelqu'un s'en serve. C'est le seul cas ou reparer est
	// gratuit — il n'y a aucune mesure a refaire derriere.
	// =========================================================================
	void NkICommandBuffer::ClearTexture(NkTextureHandle texture, const NkClearValue &value, uint32 baseMip,
										uint32 mipCount, uint32 baseLayer, uint32 layerCount) {
		(void)texture;
		(void)value;
		(void)baseMip;
		(void)mipCount;
		(void)baseLayer;
		(void)layerCount;

		// Meme cadence que ClearBuffer : les 8 premiers appels, puis un sur 10 000.
		static uint64 sAppels = 0;
		++sAppels;

		if (sAppels <= 8 || (sAppels % 10000) == 0) {
			logger.Warnf("[NkRHI] ClearTexture NON IMPLEMENTE sur ce backend : l'appel ne met RIEN a zero "
						 "(appel #%llu). AUCUN des six backends ne le surcharge a ce jour. Si vous lisez ce "
						 "message, vous etes le premier appelant : implementez la surcharge de votre backend "
						 "(Vulkan : vkCmdClearColorImage / vkCmdClearDepthStencilImage) plutot que de retirer "
						 "cet avertissement.",
						 (unsigned long long)sAppels);
		}
	}

} // namespace nkentseu
