#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaPont.h
// @Brief   LE PONT de NKScena : LA FRISE partagee (NKEditorKit, NkTimelineModel)
//          <-> LA SEQUENCE de Noge (NkSequence, format .nkseq). C'est le « seul
//          travail restant » que nommait la feuille de route (ROADMAP.md §6),
//          ecrit sur le modele de NkFriseDepuisClip / NkClipDepuisFrise
//          d'UnkenyEditor (NkEditeurPagesAnim.cpp).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QUI VA OU
//   frise                                   sequence de Noge
//   ─────────────────────────────────────   ──────────────────────────────────────
//   objet « Joueur », piste « Position »    NkTrack Transform, entityName « Joueur »,
//     (3 canaux X Y Z)                        canaux localPosition.x / .y / .z
//   piste « Rotation » (degres)             canaux localRotation.x / .y / .z
//   piste « Échelle »                       canaux localScale.x / .y / .z
//   piste de CLIPS « Plans caméra »         NkCameraTrack : un clip = un plan
//     (un clip nomme d'apres sa camera)       (cameraName, debut, duree, fondu)
//   marqueurs, plage de lecture             NkMarker, NkRenderOutput start/end
//
// LA FRISE EST LA SURFACE D'EDITION, LA SEQUENCE CE QUI JOUE ET S'ENREGISTRE :
// a chaque changement de la frise (`revision`), la sequence est refaite par
// NkScenaSequenceDepuisFrise ; a l'ouverture d'un .nkseq, la frise est refaite
// par NkScenaFriseDepuisSequence. Les deux sont DETERMINISTES : refaire la
// sequence d'une frise relue donne les MEMES octets (le banc le prouve, s3).
//
// ⚠️ CE QUE LA FRISE NE MONTRE PAS ENCORE (pistes Animation, NLA, lumiere...)
//    N'EST PAS PERDU : il passe par `reste`, rendu tel quel a l'enregistrement
//    (« une application garde intactes les sections qu'elle ne connait pas »,
//    CONVENTIONS_FICHIERS.md §6).
//
// ⚠️ Cet en-tete ne montre AUCUN type du sequenceur (declarations anticipees) :
//    `Noge/Sequencer/NkSequencer.h` ouvre `using namespace ecs;`, qui casse
//    NKRHI dans toute unite qui l'inclut avant lui (Noge/ROADMAP.md, 13/09).
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkTimelineModel.h"

namespace nkentseu {

	class NkSequence;

	namespace nkscena {

		/// Les PROPRIETES que NKScena pose sur une entite (le libelle de la piste).
		constexpr const char *kNkScenaPosition = "Position";
		constexpr const char *kNkScenaRotation = "Rotation";
		constexpr const char *kNkScenaEchelle = "Échelle";
		/// La piste des plans camera (une piste de CLIPS, a la racine de la frise).
		constexpr const char *kNkScenaPlans = "Plans caméra";

		/// Le PREFIXE des canaux de Noge pour une propriete (« localPosition »...),
		/// nul si la propriete n'en a pas.
		const char *NkScenaPrefixeCanal(const NkString &propriete) noexcept;
		/// La valeur d'une propriete au repos (position 0, rotation 0, echelle 1).
		float32 NkScenaValeurNeutre(const NkString &propriete) noexcept;

		/// L'interpolation de la frise -> celle de Noge (NkInterpolation), et
		/// retour. Rebond, Elastique et Recul n'existent pas dans Noge : ils jouent
		/// en Entree-sortie (`approchee` le dit).
		uint8 NkScenaInterpVersNoge(uint8 interpFrise, uint8 tangente, bool *approchee = nullptr) noexcept;
		uint8 NkScenaInterpDepuisNoge(uint8 interpNoge) noexcept;

		/// La frise -> la sequence. `reste` : les pistes que la frise ne montre pas
		/// (rendues apres les siennes). Le nom, la scene, les reglages de sortie
		/// de `sortie` sont GARDES (seules les pistes, plans, marqueurs, la duree,
		/// les images par seconde et la plage sont refaits). Ne RELIE pas les
		/// entites : `NkSequence::BindByName` le fait, sur le monde.
		void NkScenaSequenceDepuisFrise(const editorkit::NkTimelineModel &frise, const NkSequence *reste,
										NkSequence &sortie);

		/// La sequence -> la frise (pistes, cles, plans, marqueurs, plage). La vue de
		/// la frise (zoom, replis, piste active) est GARDEE ; l'annulation est videe.
		/// `reste` recoit ce que la frise ne sait pas montrer.
		void NkScenaFriseDepuisSequence(const NkSequence &seq, editorkit::NkTimelineModel &frise, NkSequence *reste);

		/// La valeur d'une piste de la frise a `t`, PAR L'INTERPOLATION DE NOGE (le
		/// crochet `evaluate` de la frise : la courbe peinte est celle qui jouera).
		bool NkScenaEvaluerPiste(const editorkit::NkTimelineModel &frise, const editorkit::NkTimelineTrack &piste,
								 float32 t, float32 sortie[4]) noexcept;

	} // namespace nkscena
} // namespace nkentseu
