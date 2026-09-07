#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorSurface.h
// @Brief   LA SEULE FACON DE PEINDRE AU-DESSUS — et donc la seule facon de
//          RECLAMER l'entree. Une surface flottante (menu, liste deroulante,
//          popover, palette, tiroir, modale, dialogue) obtient sa liste de
//          dessin de cet objet ; l'obtenir, c'est avoir reclame.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LE DEFAUT QU'ELLE FERME, ET POURQUOI IL EST REVENU TROIS FOIS
// =============================================================================
//  Rodolf, 06/09 : « le clic droit fait apparaitre le menu et ce menu laisse
//  traverser les evenements. Il faut corriger ce probleme sur TOUS les
//  composants qui ne doivent pas laisser traverser les evenements. »
//
//  C'est la troisieme occurrence : les dialogues de NkUIDesign, les menus du
//  menu principal de NK3DModeler, puis ce menu-ci. A chaque fois le meme
//  correctif local, a chaque fois un composant de plus qui ne l'a pas.
//
//  ⚠️ LA CAUSE N'EST PAS L'ETOURDERIE : reclamer demandait TROIS gestes
//     independants, dans trois API differentes, dont aucun n'est exige pour que
//     le dessin s'affiche. Un composant qui les oublie tous les trois a l'air
//     PARFAITEMENT NORMAL a l'ecran -- il se voit, il se survole -- et son
//     defaut ne se manifeste que dans ce qui se passe DESSOUS. C'est la lecon
//     « supprimer l'armement », mot pour mot : *un etat qu'il faut armer se fera
//     oublier par la porte que les appelants empruntent.*
//
//  LES TROIS GESTES, ET CE QUE CHACUN FAIT :
//    1. `ctx.PushOcclusion(rect, couche)` -- declare la surface au routeur
//       d'occlusion. Sans lui, `InputHits` laisse un widget de couche 0 rester
//       survolable SOUS la surface. ⚠️ La liste lue est celle de l'image
//       PRECEDENTE : la declaration doit donc etre RE-FAITE a chaque image.
//    2. `NkInputLayerScope(ctx, couche)` -- fixe la couche PENDANT le dessin.
//       Sans lui, la surface se refuse a elle-meme ses propres clics des qu'une
//       couche superieure existe, ou accepte ceux d'une couche superieure.
//    3. `ctx.input.ReserverSaisie()` -- prend souris, molette, caracteres ET
//       touches pour l'image SUIVANTE. Sans lui, la toile de l'hote a deja vu
//       le clic et la touche de la meme image : elle deselectionne, zoome et
//       supprime sous un menu ouvert.
//
//  LA PORTE : cet objet fait les trois, et il DONNE la liste de dessin. On ne
//  peut donc pas peindre au-dessus par le chemin normal du kit sans avoir
//  reclame -- ce n'est plus une etape a ne pas oublier, c'est ce qu'on tient
//  dans la main.
//
// =============================================================================
//  CE QUE CETTE PORTE NE PEUT PAS EMPECHER, ET IL FAUT LE DIRE
// =============================================================================
//  `ctx.dlOverlay` reste public -- il appartient a NKGui, et les applications
//  hors du kit l'utilisent. Un composant peut donc encore peindre au-dessus
//  sans passer par ici. Ce que la porte garantit, c'est qu'il n'y a plus AUCUNE
//  RAISON de le faire, et le banc du kit (famille 11) va LIRE LES SOURCES pour
//  le verifier : toute fonction du kit qui ecrit dans `dlOverlay` sans declarer
//  de `NkSurfaceFlottante` fait rougir le banc. Meme forme que le banc des
//  liaisons de materiau, qui va lire le shader sur le disque : la verite est
//  EXTERNE au code teste.
//
// =============================================================================
//  LE RECENSEMENT DU 2026-09-06 — DIX SURFACES, SIX ETAIENT FAUTIVES
// =============================================================================
//  Demande de Rodolf : « corriger ce probleme sur TOUS les composants ». Voici
//  donc tout ce qui, dans le kit, se peint AU-DESSUS et doit reclamer, avec son
//  etat AVANT et la porte qu'il emprunte MAINTENANT.
//
//  | # | surface                          | fichier                | avant       | maintenant          |
//  |---|----------------------------------|------------------------|-------------|---------------------|
//  | 1 | menu contextuel du kit           | NkEditorContextMenu.h  | occl+couche | porte, Menu, clavier|
//  | 2 | menu contextuel de la coquille   | NkEditorShell.cpp      | RIEN        | porte, Menu, clavier|
//  | 3 | liste deroulante (combo)         | NkEditorCombo.h        | RIEN        | porte, Menu, sans   |
//  | 4 | palette de commandes             | NkEditorShell.cpp      | occl+couche | porte, Menu, clavier|
//  | 5 | fenetre Preferences              | NkEditorShell.cpp      | RIEN        | porte, Modale, clav.|
//  | 6 | tiroir de rail                   | NkEditorShell.cpp      | RIEN        | porte, Menu, sans   |
//  | 7 | selecteur de fichiers            | NkFilePicker.h         | les trois   | porte, Modale, clav.|
//  | 8 | selecteur a deux volets          | NkFilePickerNav.h      | les trois   | porte, Modale, clav.|
//  | 9 | modale (NkModalDraw)             | NkEditorModal.h        | les trois   | A LA MAIN, dispensee|
//  |10 | cadre modal (NkModalFrameBegin)  | NkEditorModal.h        | les trois   | A LA MAIN, dispensee|
//
//  QUATRE ne reclamaient RIEN (2, 3, 5, 6) et DEUX ne faisaient que deux gestes
//  sur trois (1, 4) : six fautives sur dix. Les deux menus contextuels du kit
//  etaient dans deux etats differents -- et c'est le second, celui qui ne
//  reclamait rien, que Rodolf voyait.
//
//  ⚠️ LES DEUX DISPENSES (9 et 10) SONT ECRITES, PAS TUES. `NkEditorModal.h`
//     reclame les trois a la main et n'est pas converti : sa geometrie n'est
//     connue qu'en fin de fonction (la porte demande le rectangle a la
//     construction), et l'un de ses deux traces a un mode `inerte` ou la
//     reclamation doit etre OMISE. Convertir sans pouvoir le voir a l'ecran
//     aurait risque deux modales SAINES pour de l'uniformite. Le banc (11h)
//     verifie que la dispense reste VRAIE : si quelqu'un retire l'un des trois
//     gestes la-bas, elle cesserait de couvrir un code correct pour couvrir un
//     defaut, et l'essai rougit.
//
//  ⚠️ HORS RECENSEMENT, ET C'EST VOLONTAIRE : `NkTooltip` (NkEditorTooltip.h).
//     Il ne peint pas lui-meme -- il delegue a `nkgui::SetTooltip` -- et il est
//     NON INTERACTIF. Une infobulle qui reclamerait l'entree serait un defaut
//     symetrique : elle rendrait inerte ce qu'elle survole. *Reclamer trop est
//     un defaut au meme titre que reclamer trop peu.*
//
//  ⚠️ CE QUE LE BANC (famille 11 de `NkFilePickerNavProbe.h`) ATTRAPE ET CE
//     QU'IL LAISSE PASSER. Il lit les sources du kit et exige qu'un fichier qui
//     touche `dlOverlay` declare une `NkSurfaceFlottante` -- donc une neuvieme
//     surface ecrite dans un fichier NEUF est attrapee. Une onzieme ecrite dans
//     un fichier qui reclame DEJA ailleurs ne l'est pas : la granularite est le
//     FICHIER, pas la fonction. C'est une limite reelle, pas un oubli.
//
// =============================================================================
//  LES COUCHES, ET POURQUOI ELLES SONT UNE ENUMERATION
// =============================================================================
//  NKGui conseille « 0 fond/panneaux · 50 menus/palettes/popovers · 100 modals ·
//  200 debug » dans un COMMENTAIRE. Un conseil en commentaire se recopie de
//  travers : c'est ainsi qu'on obtient un menu en couche 100 qui recouvre la
//  modale qui l'a ouvert. Ici, ce sont trois valeurs nommees.
// -----------------------------------------------------------------------------

#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		/// LES TROIS ETAGES, nommes une fois. Les nombres sont ceux que NKGui
		/// conseille -- ils ne sont pas nouveaux, ils cessent seulement d'etre
		/// recopies a la main.
		enum class NkCouche : int32 {
			Menu = 50,	   ///< menus contextuels, listes deroulantes, popovers, palettes
			Modale = 100,  ///< dialogues, fenetres de preferences, tiroirs modaux
			Debogage = 200 ///< incrustations de mesure
		};

		/// LE CLAVIER EST UNE DECISION, PAS UN DEFAUT. Une modale le prend ; une
		/// infobulle ne le prend jamais ; un menu contextuel le prend (fleches,
		/// Echap, molette). L'ecrire au site de la declaration force a repondre.
		enum class NkPriseClavier : uint8 {
			Non = 0, ///< la surface ne lit aucune touche -- l'hote garde son clavier
			Oui = 1	 ///< la surface possede souris, molette, caracteres ET touches
		};

		// ── LE REGISTRE DE L'IMAGE ──────────────────────────────────────────────
		// Ce que le banc interroge, et ce que l'hote peut journaliser. Il compte
		// ce qui est passe PAR LA PORTE ; il ne peut pas compter ce qui est passe
		// a cote -- c'est pourquoi le banc lit AUSSI les sources.
		struct NkRegistreSurfaces {
				uint32 ouvertes = 0;	///< surfaces declarees depuis le dernier `Reinitialiser`
				uint32 avecClavier = 0; ///< dont celles qui ont pris le clavier
				int32 coucheMax = 0;	///< la plus haute couche declaree

				void Reinitialiser() noexcept {
					ouvertes = 0;
					avecClavier = 0;
					coucheMax = 0;
				}
		};

		inline NkRegistreSurfaces &NkSurfacesDeLImage() noexcept {
			static NkRegistreSurfaces r;
			return r;
		}

		// ── LA PORTE ────────────────────────────────────────────────────────────
		/// RAII. Construire = reclamer. `dl` est la liste de dessin de la couche
		/// superieure : on ne l'obtient pas autrement dans le kit.
		///
		/// ⚠️ LE RECTANGLE EST CELUI QU'ON OCCUPE REELLEMENT, pas celui qu'on
		///    aimerait. Une modale declare l'ECRAN ENTIER (elle bloque tout) ; un
		///    menu declare sa boite. Declarer trop grand rend inertes des widgets
		///    qui devraient repondre ; trop petit laisse traverser.
		struct NkSurfaceFlottante {
				NkGuiContext &ctx;
				NkGuiDrawList &dl; ///< LA liste de dessin -- la raison d'exister de cet objet
				NkRect zone;
				int32 couche;
				int32 coucheSauvee;

				NkSurfaceFlottante(NkGuiContext &c, const NkRect &r, NkCouche niveau,
								   NkPriseClavier clavier) noexcept
					: ctx(c), dl(c.dlOverlay), zone(r), couche((int32)niveau),
					  coucheSauvee(c.curInputLayer) {
					// 1. le routeur d'occlusion (lu a l'image SUIVANTE : d'ou la
					//    re-declaration a chaque image, que le RAII rend automatique)
					ctx.PushOcclusion(zone, couche);
					// 2. la couche PENDANT le dessin
					ctx.curInputLayer = couche;
					// 3. souris, molette, caracteres et touches, pour l'image suivante
					if (clavier == NkPriseClavier::Oui)
						ctx.input.ReserverSaisie();
					NkRegistreSurfaces &reg = NkSurfacesDeLImage();
					++reg.ouvertes;
					if (clavier == NkPriseClavier::Oui)
						++reg.avecClavier;
					if (couche > reg.coucheMax)
						reg.coucheMax = couche;
				}

				~NkSurfaceFlottante() noexcept {
					ctx.curInputLayer = coucheSauvee;
				}

				NkSurfaceFlottante(const NkSurfaceFlottante &) = delete;
				NkSurfaceFlottante &operator=(const NkSurfaceFlottante &) = delete;

				/// LE SURVOL, occlusion comprise. C'est `ctx.InputHits`, et le rappeler
				/// ici n'est pas de la commodite : un composant qui ecrit
				/// `NkGuiRectContains(r, mousePos)` a la main refait exactement le
				/// hit-test brut que le routeur existe pour remplacer.
				bool Survole(const NkRect &r) const noexcept {
					return ctx.InputHits(r);
				}

				bool Clic(const NkRect &r) const noexcept {
					return ctx.ClickIn(r);
				}

				/// LE GESTE « FERMER » : un clic HORS de la surface. Il est ici parce
				/// que chaque flottante le reecrivait, et que chacune l'ecrivait un peu
				/// differemment -- l'une oubliait le bouton droit, l'autre l'ancre du
				/// declencheur.
				bool ClicDehors() const noexcept {
					const NkVec2 &m = ctx.input.mousePos;
					const bool dedans = NkGuiRectContains(zone, m);
					return !dedans
						   && (ctx.input.mouseClicked[0] || ctx.input.mouseClicked[1]);
				}
		};

		// ── OU SE POSE UNE SURFACE FLOTTANTE, PAR RAPPORT A CE QUI L'OUVRE ──────
		//
		// 🔴 RETOUR DE RODOLF (07/09), sur le selecteur de couleur : il « deborde du
		//    panneau et recouvre le canvas » au lieu de rester ancre sous la
		//    pastille. MESURE : NKGui place SON propre selecteur (`ColorEdit4`)
		//    sous l'ancre -- `{sw.x, sw.y + h + 2}` -- et le retourne vers le haut
		//    s'il n'y a pas la place dessous. NkUIDesign en portait une AUTRE
		//    version, `{sw.x - largeur - 8, ...}`, qui pousse la fenetre 212 px A
		//    GAUCHE, c'est-a-dire par-dessus la toile.
		//
		// ⚠️ TROIS COPIES DE LA MEME EXPRESSION, mot pour mot, dans le meme fichier
		//    (selecteur generique, popover de remplissage, popover de bordure). La
		//    porte existait deja -- elle n'etait ecrite nulle part. *Une deuxieme
		//    copie d'une regle diverge de la premiere* : celles-ci n'ont pas diverge
		//    entre elles, elles ont diverge de la BIBLIOTHEQUE.
		//
		// ⚠️ CE QU'ON AJOUTE A LA REGLE DE NKGUI, ET POURQUOI : LE RABATTEMENT EN X.
		//    NKGui laisse sa fenetre depasser a droite ; ses selecteurs vivent dans
		//    des panneaux larges. Ici l'ancre est a ~12 px du bord de l'inspecteur,
		//    lui-meme colle au bord droit de l'ecran : sans rabattement, la moitie
		//    de la fenetre sortirait de la vue. On la RENTRE, on ne la deporte pas.
		//
		// ⚠️ ET LE COTE EST UNE INTENTION DECLAREE, PAS UNE DIVERGENCE. Mesure du
		//    07/09 : trois essais de ce depot (60b, 60c, 86-88) EXIGENT que les
		//    popovers de remplissage et de bordure s'ouvrent A GAUCHE de leur
		//    pastille -- ils sont larges (236 px) et hauts, et ce placement a ete
		//    choisi puis eprouve. Ma premiere version les a tous alignes sous
		//    l'ancre et les a fait rougir : **j'avais transforme << une regle
		//    ecrite trois fois >> en << une seule regle pour trois besoins >>**, ce
		//    qui n'est pas la meme chose. Une porte, deux intentions nommees, et un
		//    SEUL rabattement dans la vue -- c'est ce qui etait recopie.
		enum class NkCoteAncre : uint8 {
			Dessous = 0, ///< la regle de NKGui : sous l'ancre, alignee a gauche sur elle
			AGauche = 1	 ///< a gauche de l'ancre : pour les surfaces larges (popovers)
		};

		/// Place une surface flottante contre son ancre, du cote demande, retournee
		/// si elle ne tient pas, et RENTREE DANS LA VUE dans les deux axes.
		inline NkRect NkPlacerPresDeLAncre(const NkRect &ancre, float32 w, float32 h, float32 vueW,
										   float32 vueH, NkCoteAncre cote) noexcept {
			NkRect r = (cote == NkCoteAncre::AGauche)
						   ? NkRect{ancre.x - w - 8.f, ancre.y - 8.f, w, h}
						   : NkRect{ancre.x, ancre.y + ancre.h + 2.f, w, h};
			if (cote == NkCoteAncre::Dessous && r.y + r.h > vueH)
				r.y = ancre.y - r.h - 2.f; // pas la place dessous : on retourne au-dessus
			if (r.y + r.h > vueH)
				r.y = vueH - r.h - 2.f; // ni dessous ni dessus : la vue est plus courte
			if (r.y < 2.f)
				r.y = 2.f;
			if (r.x + r.w > vueW - 2.f)
				r.x = vueW - r.w - 2.f;
			if (r.x < 2.f)
				r.x = 2.f;
			return r;
		}

	} // namespace editorkit
} // namespace nkentseu
