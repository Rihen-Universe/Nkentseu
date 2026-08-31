#pragma once
// -----------------------------------------------------------------------------
// @File    Historique.h
// @Brief   L'ANNULATION UNIFIEE de NkUIDesign (§7 : « undo/redo unifié sur les
//          trois vues — une action Behavior est annulable comme une action
//          Design »). Un historique d'INSTANTANES : chaque pas est la
//          SERIALISATION COMPLETE du document — celle du round-trip, deja
//          prouvee octet pour octet. Annuler RESTAURE exactement (pas de delta
//          inverse a maintenir : la regle « au bit pres » de NkEditMesh,
//          transposee). Un document fait ~13 Ko : cent instantanes pesent
//          ~1,3 Mo — l'instantane complet suffit largement, mesure avant de
//          compliquer (le delta de serialisation attendra un document qui le
//          justifie).
//
//          ⚠️ UN GESTE = UN PAS, ET C'EST L'OBSERVATEUR QUI LE GARANTIT. Le
//          document n'est pas photographie a chaque ecriture (un drag ecrirait
//          un pas par pixel) : il est OBSERVE — l'instantane n'est pousse que
//          quand la serialisation est STABLE depuis quelques images (le geste
//          est fini : la souris a lache, la frappe fait une pause). Un drag
//          entier = un pas ; une salve de frappe = un pas par pause ; une
//          transposition = un pas. Aucun site d'edition n'a de fil a tirer —
//          les gestes FUTURS (Behavior, Animation) seront annulables sans une
//          ligne de plus : c'est le sens d'« unifie dans le MODELE ».
//
//          ⚠️ CE QUE L'HISTORIQUE NE PORTE PAS : la vue (zoom/pan), la
//          selection, l'etat des panneaux — l'annulation restaure le DOCUMENT,
//          pas la session (Figma/Lunacy pareil). La greffe IA est subsumee :
//          elle ecrit le document comme tout le monde, son pas s'annule comme
//          tout le monde (Retract reste le geste SEMANTIQUE « retirer cette
//          greffe », l'historique est le geste TEMPOREL « reviens en arriere »).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include <NKGui/NKGui.h>

namespace nkuidesign {

	using nkentseu::int32;
	using nkentseu::uint32;
	using nkentseu::NkString;
	using nkentseu::NkVector;

	class NkHistorique {
		public:
			/// Le plafond : au-dela, le plus VIEUX pas tombe (annuler cent
			/// gestes en arriere suffit a quiconque teste ; la memoire reste
			/// bornee ~1,3 Mo au poids mesure d'un document).
			static constexpr uint32 kMax = 100;

			/// Reposer l'historique sur un etat 0 (chargement, nouveau
			/// document) : tout ce qui precede est oublie — on n'annule pas a
			/// travers un rechargement.
			void Reinitialiser(const NkString &etat0) {
				mEtats.Clear();
				mEtats.PushBack(etat0);
				mCurseur = 0;
			}
			bool Vide() const { return mEtats.Size() == 0; }

			/// Pousser un nouveau pas. Identique au pas courant = refus silencieux
			/// (l'observateur repasse souvent) ; un passe « redo » existant est
			/// TRONQUE (l'histoire a bifurque — le standard de tous les editeurs).
			/// Rend vrai si un pas a reellement ete ajoute.
			bool Pousser(const NkString &etat) {
				if (mEtats.Size() > 0 && Egal(mEtats[(uint32)mCurseur], etat))
					return false;
				while ((int32)mEtats.Size() > mCurseur + 1)
					mEtats.PopBack();
				mEtats.PushBack(etat);
				if ((uint32)mEtats.Size() > kMax) {
					// le plus vieux tombe : decaler (cent elements, rare — le
					// cout ne se voit pas)
					NkVector<NkString> garde;
					garde.Reserve(mEtats.Size() - 1);
					for (uint32 i = 1; i < (uint32)mEtats.Size(); ++i)
						garde.PushBack(mEtats[i]);
					mEtats = garde;
				}
				mCurseur = (int32)mEtats.Size() - 1;
				return true;
			}

			bool PeutAnnuler() const { return mCurseur > 0; }
			bool PeutRetablir() const { return mCurseur + 1 < (int32)mEtats.Size(); }

			/// Reculer d'un pas et rendre l'etat a restaurer (nullptr si rien).
			const NkString *Annuler() {
				if (!PeutAnnuler())
					return nullptr;
				--mCurseur;
				return &mEtats[(uint32)mCurseur];
			}
			/// Avancer d'un pas et rendre l'etat a restaurer (nullptr si rien).
			const NkString *Retablir() {
				if (!PeutRetablir())
					return nullptr;
				++mCurseur;
				return &mEtats[(uint32)mCurseur];
			}

			/// ── L'OBSERVATEUR (un geste = un pas) ────────────────────────────
			/// A appeler a CHAQUE image avec la serialisation courante : pousse
			/// quand elle est STABLE depuis `kStable` passages ET differente du
			/// pas courant. Rend vrai quand un pas vient d'etre pousse.
			bool Observer(const NkString &serialisation) {
				if (mEtats.Size() == 0) {
					Reinitialiser(serialisation);
					return false;
				}
				if (!Egal(mObserve, serialisation)) {
					mObserve = serialisation;
					mStable = 0;
					return false;
				}
				if (mStable <= kStable && ++mStable == kStable
					&& !Egal(mEtats[(uint32)mCurseur], serialisation))
					return Pousser(serialisation);
				return false;
			}

		private:
			static constexpr uint32 kStable = 6; ///< ~0,1 s a 60 Hz : la pause qui clot un geste

			static bool Egal(const NkString &a, const NkString &b) {
				const char *pa = a.Data() ? a.Data() : "";
				const char *pb = b.Data() ? b.Data() : "";
				while (*pa && *pa == *pb) {
					++pa;
					++pb;
				}
				return *pa == *pb;
			}

			NkVector<NkString> mEtats;
			int32 mCurseur = -1;
			NkString mObserve; ///< derniere serialisation vue par l'observateur
			uint32 mStable = 0;
	};

} // namespace nkuidesign
