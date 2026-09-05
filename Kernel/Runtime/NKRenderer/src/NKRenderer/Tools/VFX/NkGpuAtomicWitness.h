#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGpuAtomicWitness.h — le TÉMOIN des atomiques sur tampon de NkSL (2026-09-05).
// Un noyau NkSL fait `atomicAdd(C.c[0], 1u)` depuis N invocations : le compteur doit
// valoir exactement N sur le device courant (GL, Vulkan ; DX/Metal si le banc les
// atteint). La MUTATION remplace l'atomique par `C.c[0] = C.c[0] + 1u` : le compte doit
// être faux -- un banc qui ne peut pas rougir n'est pas un banc. Second témoin :
// `atomicMax(C.c[1], i)` doit valoir N - 1.
// Pourquoi ici (NKRenderer/Tools/VFX) et pas dans la suite de NkSL : NkSL n'a pas de
// device ; NkSLComputeCheck vérifie le TEXTE sur six cibles, ce témoin vérifie le COMPTE.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
	namespace renderer {

		struct NkGpuAtomicResult {
				bool ran = false;		 // le noyau a compilé et tourné
				uint32 add = 0;			 // compteur atomicAdd (attendu : invocations)
				uint32 max = 0;			 // compteur atomicMax (attendu : invocations - 1)
				const char *why = "";	 // pourquoi ça n'a pas tourné
		};

		// mutation = true : `+=` ordinaire à la place de atomicAdd (le compte doit être faux).
		NkGpuAtomicResult NkGpuAtomicWitness(NkIDevice *device, uint32 invocations, bool mutation);

	} // namespace renderer
} // namespace nkentseu
