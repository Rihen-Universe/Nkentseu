// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAnimaActions.cpp — CE QUE FONT LES COMMANDES NOMMÉES.
// =============================================================================
//  Le document dit QUELS boutons, OÙ, avec QUEL libellé, et sous QUEL NOM. Il
//  ne dit pas ce qu'ils font, et il ne le dira jamais : appeler
//  `AnimInsertKeyAtCursor()` est du code, pas de la donnée.
//
//  ⚠️ NkAnimaEditor VISE UN ATELIER D'ANIMATION 3D (Rodolf, 26/09) — squelette,
//     courbes, VFX, animation non squelettique — de la famille Blender /
//     Cascadeur / iClone. Les actions ci-dessous sont celles que la façade
//     `AnimBridge.h` rend possibles AUJOURD'HUI. Le reste de l'atelier est
//     écrit et grisé dans les documents, jamais simulé ici.
// =============================================================================

#include "NkAnimaActions.h"

#include "AnimBridge.h"

namespace nkanima {

	namespace {

		// ── L'ÉDITION ──────────────────────────────────────────────────────
		void ActAnnuler(void *) {
			AnimUndo();
		}
		void ActRefaire(void *) {
			AnimRedo();
		}
		void ActInserer(void *) {
			AnimInsertKeyAtCursor();
		}
		void ActSupprimer(void *) {
			AnimDeleteSelected();
		}

		// ── LE TRANSPORT ───────────────────────────────────────────────────
		void ActJouer(void *) {
			AnimSetPlaying(!AnimIsPlaying());
		}
		void ActDebut(void *) {
			AnimSeek(0.f);
		}
		void ActFin(void *) {
			const float32 d = AnimDuration();
			if (d > 0.f)
				AnimSeek(d);
		}

		// ⚠️ UN PAS D'IMAGE, PAS UN PAS DE TEMPS ARBITRAIRE. `AnimFps()` donne
		//    la cadence du clip ; un pas fixe de 1/30 s sauterait des images
		//    sur un clip à 24 et en répéterait sur un clip à 60. Si la cadence
		//    est absente ou absurde, on ne devine pas : on ne bouge pas.
		void PasImage(float32 sens) {
			const float32 fps = AnimFps();
			const float32 duree = AnimDuration();
			if (!(fps > 0.f) || !(duree > 0.f))
				return;
			float32 t = AnimCursor() + sens / fps;
			if (t < 0.f)
				t = 0.f;
			if (t > duree)
				t = duree;
			AnimSeek(t);
		}
		void ActImagePrec(void *) {
			PasImage(-1.f);
		}
		void ActImageSuiv(void *) {
			PasImage(+1.f);
		}

		// ── LA SÉLECTION ───────────────────────────────────────────────────
		//  ⚠️ « Tout sélectionner » N'EST PAS ÉCRIT ICI. La façade n'offre que
		//     `AnimSelectKey(t)`, clé par clé. Boucler sur `AnimGetKeyTimes`
		//     depuis une action inventerait une politique de sélection alors
		//     qu'elle appartient au modèle — et il faudrait trancher si « tout »
		//     veut dire toutes les clés, celles du joint courant, ou celles de
		//     la plage visible. Trois réponses, aucune écrite nulle part.
		void ActSelectionRien(void *) {
			AnimClearSelection();
		}

		// ── L'ÉDITION DE POSE (le geste de Cascadeur) ──────────────────────
		//  Trois portes distinctes, et elles ne se valent pas : entrer,
		//  ENREGISTRER une clé depuis la pose, et sortir SANS enregistrer. Les
		//  confondre ferait perdre du travail en silence.
		void ActPoseEntrer(void *) {
			if (!AnimInPoseEdit())
				AnimBeginPoseEdit();
		}
		void ActPoseEnregistrer(void *) {
			if (AnimInPoseEdit())
				AnimCommitPoseKey();
		}
		void ActPoseQuitter(void *) {
			if (AnimInPoseEdit())
				AnimEndPoseEdit();
		}

		// ── LES TROIS MODES D'AFFICHAGE, comme dans Blender ────────────────
		void ActVueSolide(void *) {
			Anim3DSetViewMode(NkAnimViewMode::SOLIDE);
		}
		void ActVueRendu(void *) {
			Anim3DSetViewMode(NkAnimViewMode::RENDU);
		}
		void ActVueFilaire(void *) {
			Anim3DSetViewMode(NkAnimViewMode::FILAIRE);
		}

		void ActCompteurs(void *) {
			Anim3DBasculerCompteurs();
		}

		// ── LES BASCULES SERVIES PAR UN `behavior` DE DOCUMENT ─────────────
		//  ⚠️ IDEMPOTENTES PAR OBLIGATION. L'hôte passe TOUS les `behavior` une
		//     fois par image (limite 5 de `NkGuiInteraction.h`, le format n'a
		//     pas encore d'événement). Une bascule ici serait un clignotant à
		//     60 Hz.
		//
		//  ⚠️ ET LA CASE DEVIENT LA SEULE VÉRITÉ. Tant que le document repose la
		//     valeur à chaque image, un autre chemin qui changerait la physique
		//     se verrait écrasé au tour suivant. C'est acceptable parce que la
		//     case EST le contrôle ; ça cessera de l'être le jour où un
		//     raccourci clavier fera la même bascule. Il faudra alors
		//     l'événement que le format n'a pas — c'est écrit pour qu'on s'en
		//     souvienne.

		// ⚠️ ÉCRIT MAIS JAMAIS LU AUJOURD'HUI. `LectureEnBoucle()` existe pour
		//    que l'état soit lisible, mais RIEN ne l'interroge encore : le
		//    rebouclage est obtenu par `ActBoucleActivee`, rejouée à chaque
		//    image, qui ramène le curseur à zéro quand la lecture atteint la
		//    fin. Le dire plutôt que de laisser croire à un drapeau consommé.
		bool gBoucle = false;

		void ActBoucleActivee(void *) {
			gBoucle = true;
			const float32 d = AnimDuration();
			if (AnimIsPlaying() && d > 0.f && AnimCursor() >= d - 0.001f)
				AnimSeek(0.f);
		}
		void ActBoucleCoupee(void *) {
			gBoucle = false;
		}

		void ActPhysiqueOn(void *) {
			AnimSetPhysics(true);
		}
		void ActPhysiqueOff(void *) {
			AnimSetPhysics(false);
		}
		void ActComOn(void *) {
			AnimSetShowCOM(true);
		}
		void ActComOff(void *) {
			AnimSetShowCOM(false);
		}

		// ═══════════════════════════════════════════════════════════════════
		//  LA TABLE — fermée, et son compte se DÉDUIT
		// ═══════════════════════════════════════════════════════════════════
		//  ⚠️ LE NOMBRE NE SE RECOPIE PAS. NK3DModeler a payé exactement ça :
		//     ajouter « Enregistrer tout » à un menu sans toucher le compte
		//     écrit à la main a fait DISPARAÎTRE « Quitter ». Ici le compte est
		//     calculé au même endroit que la table, et l'accesseur est le seul
		//     chemin vers les deux.
		const nkgui::NkActionNommee kActions[] = {
			// l'édition
			{"anim.annuler", &ActAnnuler, nullptr},
			{"anim.refaire", &ActRefaire, nullptr},
			{"anim.inserer", &ActInserer, nullptr},
			{"anim.supprimer", &ActSupprimer, nullptr},
			// le transport
			{"anim.jouer", &ActJouer, nullptr},
			{"anim.debut", &ActDebut, nullptr},
			{"anim.fin", &ActFin, nullptr},
			{"anim.image_prec", &ActImagePrec, nullptr},
			{"anim.image_suiv", &ActImageSuiv, nullptr},
			// la sélection
			{"anim.selection_rien", &ActSelectionRien, nullptr},
			// la pose
			{"anim.pose_entrer", &ActPoseEntrer, nullptr},
			{"anim.pose_enregistrer", &ActPoseEnregistrer, nullptr},
			{"anim.pose_quitter", &ActPoseQuitter, nullptr},
			// la vue
			{"anim.vue_solide", &ActVueSolide, nullptr},
			{"anim.vue_rendu", &ActVueRendu, nullptr},
			{"anim.vue_filaire", &ActVueFilaire, nullptr},
			// (26/09) LA MÊME FONCTION QUE LA PALETTE, PAS UNE SECONDE. La
			// barre du document et la palette de commandes entrent par la MÊME
			// porte. Deux chemins vers deux fonctions jumelles auraient divergé
			// — ce dépôt a payé *deux compteurs sans code commun*.
			{"anim.compteurs", &ActCompteurs, nullptr},
			// les bascules de document
			{"anim.boucle_activee", &ActBoucleActivee, nullptr},
			{"anim.boucle_coupee", &ActBoucleCoupee, nullptr},
			{"anim.physique_on", &ActPhysiqueOn, nullptr},
			{"anim.physique_off", &ActPhysiqueOff, nullptr},
			{"anim.com_on", &ActComOn, nullptr},
			{"anim.com_off", &ActComOff, nullptr},
		};

	} // namespace

	const nkgui::NkActionNommee *ActionsAnimation(uint32 &outNombre) noexcept {
		outNombre = (uint32)(sizeof(kActions) / sizeof(kActions[0]));
		return kActions;
	}

	nkgui::NkActionFn FonctionDe(const char *nom) noexcept {
		if (!nom)
			return nullptr;
		const uint32 n = (uint32)(sizeof(kActions) / sizeof(kActions[0]));
		for (uint32 i = 0; i < n; ++i) {
			const char *a = kActions[i].nom;
			const char *b = nom;
			while (*a && *a == *b) {
				++a;
				++b;
			}
			if (*a == '\0' && *b == '\0')
				return kActions[i].fn;
		}
		return nullptr;
	}

	bool LectureEnBoucle() noexcept {
		return gBoucle;
	}

} // namespace nkanima
