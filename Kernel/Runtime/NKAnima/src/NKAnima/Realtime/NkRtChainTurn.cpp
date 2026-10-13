// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtChainTurn.cpp — une rotation repartie sur une chaine d'os (voir
// NkRtChainTurn.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtChainTurn.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {
		namespace rt {

			NkRtQuat NkRtTurnRotation(const NkRtTurn &turn, float32 share) noexcept {
				return QMul(QMul(QFromAxisAngle(turn.axes[0], turn.angles[0] * share), QFromAxisAngle(turn.axes[1], turn.angles[1] * share)),
							QFromAxisAngle(turn.axes[2], turn.angles[2] * share));
			}

			NkRtQuat NkRtTurnBoneInWorld(const NkRtQuat &parentWorld, const NkRtQuat &worldRotation, const NkRtQuat &local) noexcept {
				return QMul(QMul(QMul(QConj(parentWorld), worldRotation), parentWorld), local);
			}

			void NkRtDistributeTurn(const NkRtTurn &turn, const float32 *shares, const NkRtQuat *parentWorld, const NkRtQuat *refLocal, uint32 count,
									NkRtQuat *outLocal) noexcept {
				if (shares == nullptr || parentWorld == nullptr || refLocal == nullptr || outLocal == nullptr) {
					return;
				}
				const bool sansAngle = turn.angles[0] == 0.f && turn.angles[1] == 0.f && turn.angles[2] == 0.f;
				// MUTATION turn_no_spread : rien n'est reparti -- le dernier os de la chaine
				// prend toute la rotation, les autres gardent leur reference.
				const bool seul = NkRtMutated(NK_RT_MUT_TURN_NO_SPREAD);
				for (uint32 k = 0; k < count; ++k) {
					const float32 part = seul ? (k + 1u == count ? 1.f : 0.f) : shares[k];
					if (sansAngle || part == 0.f) {
						outLocal[k] = refLocal[k]; // la reference, au bit pres
						continue;
					}
					outLocal[k] = NkRtTurnBoneInWorld(parentWorld[k], NkRtTurnRotation(turn, part), refLocal[k]);
				}
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
