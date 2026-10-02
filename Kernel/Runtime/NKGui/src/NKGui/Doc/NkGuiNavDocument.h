#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiNavDocument.h
// @Brief   LA NAVIGATION AU FOCUS SUR UN DOCUMENT `.nkgui` : ce que la
//          navigation du noyau (Core/NkGuiNavigation.h) ne peut pas savoir
//          seule, parce que c'est le DOCUMENT qui le dit.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER (02/10, l'interface en jeu d'Unkeny)
// =============================================================================
//  La navigation du noyau vise des NOTES (nature, rectangle, cle) et agit par un
//  pointeur virtuel. Deux gestes lui echappent, et ils sont ceux d'un menu de
//  jeu a la manette :
//
//   1. REGLER UN CURSEUR. Gauche / Droite sur un `Slider` focalise doit
//      deplacer sa valeur d'un PAS. Le noyau ne connait ni le pas, ni les
//      bornes : ils sont ecrits dans le document (`min`, `max`, `step`). Et un
//      clic virtuel au centre de la ligne POSERAIT la valeur au milieu -- le
//      contraire de ce qu'on veut. On regle donc la valeur dans le MAGASIN du
//      monteur (la meme entree que le geste de la souris), et l'on consomme la
//      direction et l'activation.
//
//   2. L'ANNEAU DU DOCUMENT. `appearance(FocusVisible)` est la facon dont un
//      document dessine le focus clavier ; `NkGuiExecution::DonnerFocus` le
//      porte, mais rien ne le reliait a la navigation. On le relie ici : le
//      focus pilote a la manette ou au clavier EST un focus visible.
//
//  EN-TETE SEUL, comme le monteur et la coquille : l'application qui l'inclut
//  declare NKSerialization (et NKFileSystem pour la coquille).
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiNavigation.h"
#include "NKGui/Doc/NkGuiCoquille.h"

namespace nkentseu {
	namespace nkgui {

		/// Le widget d'identifiant `id` dans `doc` (toutes sections `widgets`),
		/// ou nul. Un identifiant est unique dans un document (W-ID-DUPLICATE) :
		/// le premier trouve est le bon.
		inline const NkArchive *NkGuiTrouverWidgetDoc(const NkArchive &bloc, const char *id) noexcept {
			if (id == nullptr || id[0] == '\0') {
				return nullptr;
			}
			const NkArchiveNode *c = NkGMonteCorps(bloc);
			if (c == nullptr) {
				return nullptr;
			}
			for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
				if (!c->array[k].IsObject() || !c->array[k].object) {
					continue;
				}
				const NkArchive &w = *c->array[k].object;
				if (NkGMotEgal(NkGuiArchive::IdOf(w), id)) {
					return &w;
				}
				if (const NkArchive *dedans = NkGuiTrouverWidgetDoc(w, id)) {
					return dedans;
				}
			}
			return nullptr;
		}

		/// Le role du widget `id` (« Slider », « Button »...), vide s'il n'existe pas.
		inline NkString NkGuiRoleWidgetDoc(const NkArchive &doc, const char *id) noexcept {
			const NkArchive *w = NkGuiTrouverWidgetDoc(doc, id);
			return w != nullptr ? NkString(NkGuiArchive::TypeOf(*w)) : NkString();
		}

		/// Gauche / Droite sur un `Slider` du document focalise : la valeur avance
		/// d'un `step` (a defaut, un vingtieme de la plage), bornee. Rend VRAI si le
		/// geste a ete consomme -- `direction` passe alors a `Aucune` et `activer`
		/// a faux (un clic virtuel poserait la valeur au milieu de la ligne).
		/// Rend faux, sans rien toucher, hors d'un curseur du document.
		inline bool NkGuiNavReglerDocument(NkBandeDocument &bande, const NkGuiNavigation &nav,
										   NkGuiNavDirection &direction, bool &activer) noexcept {
			if (!nav.actif || nav.focusCle[0] == '\0' || !bande.lu) {
				return false;
			}
			const NkArchive *w = NkGuiTrouverWidgetDoc(bande.doc, nav.focusCle);
			if (w == nullptr || !NkGMotEgal(NkGuiArchive::TypeOf(*w), "Slider")) {
				return false;
			}
			// Sur un curseur, « activer » ne fait rien d'utile : on l'avale.
			activer = false;
			if (direction != NkGuiNavDirection::Gauche && direction != NkGuiNavDirection::Droite) {
				return direction == NkGuiNavDirection::Aucune;
			}
			const float32 vmin = NkGNombre(*w, "min", 0.f);
			const float32 vmax = NkGNombre(*w, "max", 1.f);
			float32 pas = NkGNombre(*w, "step", 0.f);
			if (pas <= 0.f) {
				pas = (vmax - vmin) / 20.f;
			}
			// LA MEME CLE QUE LE MONTEUR : le `bind` s'il y en a un, sinon l'id.
			NkString cle(NkGuiArchive::IdOf(*w));
			if (const NkArchiveNode *b = w->FindNode(NkStringView("bind"))) {
				cle = NkString(b->Lexeme());
			}
			NkGuiMonteEtat::Entree *e = bande.etat.Get(NkStringView(cle.CStr()));
			if (e == nullptr) {
				return false;
			}
			float32 v = e->initialise ? e->f : NkGNombre(*w, "value", vmin);
			v += direction == NkGuiNavDirection::Droite ? pas : -pas;
			if (v < vmin) {
				v = vmin;
			}
			if (v > vmax) {
				v = vmax;
			}
			e->f = v;
			e->initialise = true;
			direction = NkGuiNavDirection::Aucune;
			return true;
		}

		/// Relie le focus de la navigation au focus du DOCUMENT : pilote, il est
		/// VISIBLE (`appearance(FocusVisible)` se peint) ; rendu a la souris, il
		/// s'eteint. A appeler apres NkGuiNavAvancer, avant le montage.
		inline void NkGuiNavLierFocusDocument(NkBandeDocument &bande, const NkGuiNavigation &nav) noexcept {
			if (nav.actif && nav.pilote && nav.focusCle[0] != '\0') {
				bande.DonnerFocus(NkStringView(nav.focusCle), true);
			} else if (nav.focusCle[0] != '\0' && NkGMotEgal(bande.Focus(), nav.focusCle)) {
				// Seulement le focus que la navigation avait donne : celui d'un
				// `focus x` ecrit dans un comportement n'est pas a elle.
				bande.RetirerFocus();
			}
		}

	} // namespace nkgui
} // namespace nkentseu
