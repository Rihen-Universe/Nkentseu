#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkSondeInerte.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   UNE FENETRE DE SONDE N'ECOUTE PAS RODOLF : elle ignore toute entree
//          qu'elle n'a pas elle-meme injectee, et elle le DIT dans son journal.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (24-25/09) — DEUX FAUX DEFAUTS EN UNE NUIT
// =============================================================================
//  Une fenetre de sonde prend le focus et passe au premier plan. Rodolf continue
//  de travailler : ses clics et ses frappes atterrissent DANS LA SONDE. Deux
//  incidents le meme soir, par deux agents differents :
//
//    - une demande est partie du composeur alors que personne ne l'avait envoyee
//      (agent de la modelisation) ;
//    - le presse-papiers a ete ecrit pendant une mesure, et j'en ai conclu -- a
//      tort -- que « envoyer une demande copie dans le presse-papiers ». La
//      trace a fini par nommer le vrai site (le bouton « copier le bloc », garde
//      par un vrai clic) et la reproduction a donne 6 copies, puis 0, puis 0.
//      **Un defaut de l'envoi serait deterministe ; celui-la ne l'etait pas.**
//
//  Une sonde qui recoit de l'entree humaine ne mesure plus le produit : elle
//  fabrique des defauts qui n'existent pas, et on les corrige. C'est le piege le
//  plus cher de ce projet -- tirer une cause d'une correlation.
//
// =============================================================================
//  CE QUE CE FICHIER FAIT, ET CE QU'IL NE PEUT PAS FAIRE
// =============================================================================
//  ⚠️ IL NE PEUT PAS EMPECHER LA FENETRE DE PRENDRE LE FOCUS. `NkWindowConfig`
//     ne porte aucun drapeau « sans activation » : l'ajouter demanderait de
//     toucher le dorsal Win32 (WS_EX_NOACTIVATE) et les autres plateformes. Ce
//     n'est pas fait, et c'est dit ici plutot que laisse a decouvrir.
//
//  CE QU'IL FAIT : la seconde branche, celle qui tient sans toucher au systeme.
//  Sous `NK_SONDE` (ou `NK_TOAST_PROBE`), l'entree de NKGui est REMISE A ZERO a
//  chaque image, SAUF sur les images ou le script d'evenements a lui-meme
//  injecte quelque chose. Un clic de Rodolf n'arrive donc jamais jusqu'au
//  panneau ; un clic du script, si.
//
//  ⚠️ LA GRANULARITE EST L'IMAGE, PAS L'EVENEMENT, et c'est une limite reelle.
//     Sur une image ou le script injecte, une entree humaine simultanee passerait
//     elle aussi. Distinguer les deux demanderait de marquer chaque evenement a
//     sa naissance, ce que `NkEvent` ne porte pas. La fenetre de collision est
//     d'une image sur les quelques-unes qu'un script utilise -- contre TOUTES les
//     images aujourd'hui.
//
//  ⚠️ CE QUI EST IGNORE EST COMPTE ET DIT. `NkSondeEntreesIgnorees()` compte les
//     images ou une entree humaine a ete jetee, et la porte l'imprime la premiere
//     fois puis toutes les 120 occurrences. Une sonde qui mange l'entree en
//     silence serait la version symetrique du meme piege : on chercherait
//     pourquoi « le clic ne fait rien ».
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiContext.h"
#include "NKWindow/Core/NkWindowConfig.h" // (25/09) `noActivate` : la vraie parade au vol de focus
#include <cstdio>
#include <cstdlib>

namespace nkentseu {
	namespace editorkit {

		/// Sommes-nous une fenetre de sonde ? Lu UNE fois : une variable
		/// d'environnement ne change pas en cours de course, et la relire a chaque
		/// image ferait du comportement une chose qui peut basculer sous les pieds.
		inline bool NkSondeActive() {
			static int32 e = -1;
			if (e < 0)
				e = (std::getenv("NK_SONDE") != nullptr || std::getenv("NK_TOAST_PROBE") != nullptr) ? 1 : 0;
			return e == 1;
		}

		/// Le script d'evenements a-t-il injecte quelque chose a CETTE image ?
		/// Pose par `NkEditorScriptEvenements::Tick`, consomme par la porte.
		inline bool &NkSondeInjecteCetteImage() {
			static bool b = false;
			return b;
		}

		/// Le nombre d'IMAGES dont l'entree a ete jetee. C'est le chiffre que le
		/// rapport cite : sans lui, « la sonde est inerte » serait une affirmation.
		inline uint32 &NkSondeEntreesIgnorees() {
			static uint32 n = 0;
			return n;
		}

		/// Le nombre d'images LAISSEES PASSER parce que le script venait d'injecter.
		/// Les deux compteurs se lisent ensemble : sans celui-ci, une porte qui
		/// jetterait TOUT (script compris) serait indiscernable d'une porte qui
		/// marche -- et le script cesserait de fonctionner en silence.
		inline uint32 &NkSondeEntreesLaissees() {
			static uint32 n = 0;
			return n;
		}

		/// Y a-t-il une entree HUMAINE dans cette image ? On ne jette pas une image
		/// vide -- compter les images sans entree gonflerait le chiffre et le
		/// rendrait inutilisable.
		inline bool NkSondeEntreePresente(const nkgui::NkGuiInput &in) {
			if (in.mouseClicked[0] || in.mouseClicked[1] || in.mouseClicked[2])
				return true;
			if (in.mouseDown[0] || in.mouseDown[1] || in.mouseDown[2])
				return true;
			if (in.charCount > 0 || in.wheel != 0.f)
				return true;
			if (in.wantCopy || in.wantCut || in.wantPaste || in.wantSelectAll)
				return true;
			for (int32 k = 0; k < nkgui::NkGuiInput::KeyCount; ++k)
				if (in.keyDown[k])
					return true;
			return false;
		}

		/// (25/09) LA FENETRE DE SONDE DEMANDE-T-ELLE A NE PAS PRENDRE LE FOCUS,
		/// et la plateforme le tient-elle ? Pose `config.noActivate` et rend vrai
		/// quand le dorsal l'honore.
		///
		/// ⚠️ C'EST LA VRAIE PARADE, et la porte par image n'en est que le
		///    rattrapage. `NkSondeFiltrerEntree` jette l'entree APRES qu'elle est
		///    arrivee ; `noActivate` fait qu'elle n'arrive pas. Les deux servent :
		///    la premiere couvre ce qui entre malgre tout (une fenetre qu'on clique
		///    volontairement), la seconde empeche le vol de focus.
		///
		/// ⚠️ LE REFUS EST NOMME, PAS SILENCIEUX. Hors Win32, aucun dorsal ne
		///    porte encore l'equivalent (`_NET_WM_STATE` / `set_input_region` sous
		///    X11 et Wayland, `NSWindowStyleMaskNonactivatingPanel` sous macOS) :
		///    on le DIT au journal au lieu de laisser croire que la fenetre est
		///    discrete. *Un reglage affiche qui ne change rien est pire qu'un
		///    reglage absent.*
		inline bool NkSondePoserFenetreDiscrete(NkWindowConfig &wc) {
			if (!NkSondeActive())
				return false;
			wc.noActivate = true;
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(_WIN32)
			return true;
#else
			std::printf("[sonde] REFUS NOMME : cette plateforme ne sait pas encore ouvrir une "
						"fenetre SANS PRENDRE LE FOCUS. La sonde peut donc recevoir les clics "
						"et les frappes de l'utilisateur ; seule la porte par image les "
						"jettera, et seulement apres coup.\n");
			std::fflush(stdout);
			return false;
#endif
		}

		/// LA PORTE UNIQUE. A appeler une fois par image, APRES que l'hote a
		/// traduit les evenements dans `ctx.input` et AVANT que l'interface ne le
		/// lise. Hors sonde, elle ne fait RIEN -- pas une branche, pas un test de
		/// plus dans le produit.
		///
		/// ⚠️ LA POSITION DE LA SOURIS N'EST PAS EFFACEE, et c'est voulu : elle ne
		///    declenche rien par elle-meme, et la remettre a zero enverrait le
		///    curseur en (0,0), ce qui ferait SURVOLER le coin de la fenetre. Un
		///    survol fantome est une entree, lui aussi.
		inline void NkSondeFiltrerEntree(nkgui::NkGuiContext &ctx, const char *quiAppelle = nullptr) {
			bool &injecte = NkSondeInjecteCetteImage();
			if (!NkSondeActive()) {
				injecte = false;
				return;
			}
			if (injecte) {
				injecte = false; // consomme : l'image suivante redevient inerte
				const uint32 l = ++NkSondeEntreesLaissees();
				if (l == 1u || (l % 120u) == 0u) {
					std::printf("[sonde] entree INJECTEE laissee passer (%u image(s))\n", (unsigned)l);
					std::fflush(stdout);
				}
				return;
			}
			if (!NkSondeEntreePresente(ctx.input))
				return;
			const uint32 n = ++NkSondeEntreesIgnorees();
			// Tout ce qui DECLENCHE part. Ce qui ne fait que decrire un etat (la
			// position du curseur, le temps) reste.
			for (int32 i = 0; i < 3; ++i) {
				ctx.input.mouseDown[i] = false;
				ctx.input.mouseClicked[i] = false;
			}
			ctx.input.wheel = 0.f;
			ctx.input.charCount = 0;
			ctx.input.ctrlDown = ctx.input.shiftDown = ctx.input.altDown = false;
			ctx.input.wantCopy = ctx.input.wantCut = ctx.input.wantPaste = ctx.input.wantSelectAll = false;
			for (int32 k = 0; k < nkgui::NkGuiInput::KeyCount; ++k)
				ctx.input.keyDown[k] = false;
			if (n == 1u || (n % 120u) == 0u) {
				std::printf("[sonde] ENTREE HUMAINE IGNOREE (%u image(s) depuis le debut) -- cette fenetre "
							"est une SONDE DE MESURE, elle n'obeit qu'a son script%s%s\n",
							(unsigned)n, quiAppelle ? " ; porte : " : "", quiAppelle ? quiAppelle : "");
				std::fflush(stdout);
			}
		}

	} // namespace editorkit
} // namespace nkentseu
