#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Clip/NkClipEvents.h : LES EVENEMENTS D'UN CLIP (un pas, un son, une etincelle),
// traverses entre deux temps de lecture (14/10/2026).
//
// D'OU VIENT CE FICHIER. Le lecteur NkAnimationPlayer a des « markers », mais chacun
// ne tire qu'UNE fois par Play() : son drapeau `fired` n'est jamais remis a zero
// quand le clip boucle (NkAnimation.cpp, NkAnimationPlayer::Update). Une marche en
// boucle ne fait donc entendre que son premier pas (mesure dans le banc, EV1). Et la
// machine a etats, les arbres de melange, le sequenceur de Noge ne passent pas par
// ce lecteur. Ici, une piste d'evenements SANS ETAT : deux temps de lecture donnent
// la liste des evenements traverses, dans l'ordre ou on les rencontre.
//
// LA REGLE. Un evenement au temps te d'un clip de duree D qui boucle se produit aux
// temps de lecture te + k D. En avant, on rend ceux de ]e0, e1] : des intervalles
// d'image successifs se touchent sans se recouvrir, donc chaque evenement tire UNE
// fois, meme pose pile sur une image (et `includeStart` ajoute ceux de e0 lui-meme,
// pour la premiere image d'une lecture). A rebours (e1 < e0) : ceux de [e1, e0[,
// du plus tard au plus tot. Un clip qui ne boucle pas : ses evenements dans [0, D].
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace anim {

		// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────────
		enum NkClipEventMutation : uint32 {
			NK_EV_MUT_NONE = 0u,
			NK_EV_MUT_CLOSED = 1u << 0,	 ///< l'intervalle est ferme des deux cotes (un evenement pose sur une image tire deux fois)
			NK_EV_MUT_NO_LOOP = 1u << 1, ///< les temps sont replies, les cycles ne sont pas parcourus (le passage de la boucle perd des evenements)
		};
		void NkClipEventSetMutations(uint32 bits) noexcept;
		[[nodiscard]] uint32 NkClipEventMutations() noexcept;

		struct NkClipEvent {
				float32 time = 0.f; ///< temps du clip (secondes)
				NkString name;
		};

		class NkClipEventTrack {
			public:
				NkVector<NkClipEvent> events; ///< tries par temps (Add les range)

				/// Ajoute un evenement (insere a sa place ; deux evenements au meme temps
				/// gardent leur ordre d'ajout).
				void Add(float32 time, const NkString &name);

				/// Les evenements traverses en jouant de `elapsed0` a `elapsed1` (temps de
				/// LECTURE, non replies) d'un clip de duree `duration`, qui boucle ou non :
				/// leurs indices dans `events`, dans l'ordre de rencontre, ajoutes a `out`.
				/// Rend leur nombre.
				uint32 Collect(float32 duration, bool loop, float32 elapsed0, float32 elapsed1, NkVector<uint32> &out,
							   bool includeStart = false) const;
		};

	} // namespace anim
} // namespace nkentseu
