// =============================================================================
// NKCivilization/NkCivReplay.h — le rejoueur de trajectoires (Phase 5, outils
// d'observation) : recharge un journal `.nkciv` (NkCivRecorder) et rejoue la
// simulation TICK PAR TICK **sans re-simuler** — lecture PURE du journal
// (aucune politique évaluée, aucune règle de monde rejouée : les états sont
// LUS, pas recalculés). Le curseur avance frame par frame ; l'état courant de
// chaque agent (position, terminé ou non) est reconstitué depuis le journal.
//
// La preuve du round-trip : NkCivReplayer::VerifyExact compare frame par frame
// deux journaux (l'original en mémoire vs celui rechargé du disque) — combinée
// à la comparaison OCTET PAR OCTET des deux sérialisations dans le test, elle
// garantit que le rejeu reproduit exactement les états enregistrés.
// Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKCivilization/NkCivRecorder.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			class NkCivReplayer {
				public:
					// `journal` non possédé (durée de vie gérée par l'appelant).
					// false si le journal est nul ou vide (rien à rejouer).
					bool Open(const NkCivRecorder *journal);

					// Replace le curseur AVANT le premier tick (état = départs).
					void Reset() {
						mCursor = -1;
					}

					uint32 FrameCount() const {
						return mJournal ? mJournal->FrameCount() : 0u;
					}

					bool AtEnd() const {
						return mJournal == nullptr || mCursor + 1 >= (int32)mJournal->FrameCount();
					}

					// Avance d'UN tick (lecture pure). false si la fin est atteinte.
					bool NextTick();

					// Index de la frame courante (-1 = avant le premier tick).
					int32 Cursor() const {
						return mCursor;
					}

					// Frame courante — valide seulement après un NextTick() réussi.
					const NkCivRecFrame &CurrentFrame() const {
						return mJournal->Frame((uint32)mCursor);
					}

					// ── État reconstitué (lecture pure du journal) ───────────────────
					// Position de l'agent `turnOrder` au curseur courant (départ si le
					// curseur est avant le premier tick).
					uint32 AgentState(uint32 turnOrder) const;
					// L'agent est-il terminé (but ou trou) au curseur courant ?
					bool AgentDone(uint32 turnOrder) const;

					// ── Vérification round-trip ──────────────────────────────────────
					// true si `a` et `b` sont STRUCTURELLEMENT identiques frame par
					// frame (en-tête + tous les ticks). En cas d'écart, `outFirstDiff`
					// (optionnel) reçoit l'index de la première frame divergente
					// (0xFFFFFFFF si l'écart est dans l'en-tête).
					static bool VerifyExact(const NkCivRecorder &a, const NkCivRecorder &b,
											uint32 *outFirstDiff = nullptr);

				private:
					const NkCivRecorder *mJournal = nullptr; // non possédé
					int32 mCursor = -1;
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
