// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurAppareilsUi.cpp
// DESCRIPTION: La section « Appareil simulé » de l'onglet Monde des Details :
//              orientation, interrupteurs, provenance, et l'appareil
//              PERSONNALISE (document 03, §2.2).
//
// ⚠️ L'APPAREIL PERSONNALISE NE S'ECRIT PAS EN MARGES
//   On y regle ce qui mange l'ecran (barre d'etat, decoupe, indicateur,
//   navigation) et le systeme ; les marges en sont DEDUITES par
//   NkMargesSysteme, dans les quatre orientations. Des marges tapees a la main
//   seraient justes en portrait et fausses des la premiere rotation.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkGuiContext;

		namespace {

			/// Une rangee de boutons, celui de la valeur courante grise.
			bool Choix(NkGuiContext &ctx, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", libelle, noms[valeur >= 0 && valeur < n ? valeur : 0]).CStr());
				bool change = false;
				ctx.PushId(libelle);
				for (int32 i = 0; i < n; ++i) {
					if (i > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(i == valeur);
					if (nkgui::Button(ctx, noms[i])) {
						valeur = i;
						change = true;
					}
					ctx.EndDisabled();
				}
				ctx.PopId();
				return change;
			}

			/// L'appareil personnalise, champ par champ. Les bornes empechent un
			/// appareil impossible (largeur nulle, decoupe hors de l'ecran).
			void Personnalise(NkGuiContext &ctx, NkEditeurModele &m) {
				NkProfilAppareil &p = m.appareil.perso;
				ctx.PushId("perso");
				nkgui::Text(ctx, NkString::Format("type de base : %s", NkProfil(m.appareil.persoBase).nom).CStr());
				static const char *kSystemes[4] = {"aucun", "iOS", "Android", "Web"};
				int32 sys = static_cast<int32>(p.systeme);
				if (Choix(ctx, "système", kSystemes, 4, sys)) {
					p.systeme = static_cast<NkSystemeAppareil>(sys);
				}
				int32 l = static_cast<int32>(p.largeur);
				int32 h = static_cast<int32>(p.hauteur);
				nkgui::InputInt(ctx, "largeur (pt)", l, 1);
				nkgui::InputInt(ctx, "hauteur (pt)", h, 1);
				p.largeur = static_cast<uint32>(l < 100 ? 100 : (l > 4000 ? 4000 : l));
				p.hauteur = static_cast<uint32>(h < 100 ? 100 : (h > 4000 ? 4000 : h));
				nkgui::SliderFloat(ctx, "densité (px/pt)", p.densite, 1.f, 4.f);
				const float32 demi = static_cast<float32>(p.largeur < p.hauteur ? p.largeur : p.hauteur) * 0.5f;
				nkgui::Checkbox(ctx, "écran rond", p.rond);
				if (!p.rond) {
					nkgui::SliderFloat(ctx, "rayon des coins (pt)", p.rayonCoins, 0.f, demi);
				}
				nkgui::Separator(ctx);
				static const char *kDecoupes[5] = {"aucune", "encoche", "îlot", "poinçon", "goutte"};
				int32 dec = static_cast<int32>(p.decoupe);
				if (Choix(ctx, "découpe", kDecoupes, 5, dec)) {
					p.decoupe = static_cast<NkTypeDecoupe>(dec);
					// Une forme choisie sans taille ne se verrait pas : une taille
					// de depart, centree en haut.
					if (p.decoupe != NkTypeDecoupe::NK_AUCUNE && (p.rectDecoupe.w < 1.f || p.rectDecoupe.h < 1.f)) {
						p.rectDecoupe = NkRectAppareil{static_cast<float32>(p.largeur) * 0.5f - 15.f, 8.f, 30.f, 30.f};
					}
				}
				if (p.decoupe != NkTypeDecoupe::NK_AUCUNE) {
					nkgui::SliderFloat(ctx, "découpe : largeur", p.rectDecoupe.w, 4.f, static_cast<float32>(p.largeur) * 0.8f);
					nkgui::SliderFloat(ctx, "découpe : hauteur", p.rectDecoupe.h, 4.f, 80.f);
					nkgui::SliderFloat(ctx, "découpe : position x", p.rectDecoupe.x, 0.f,
									   static_cast<float32>(p.largeur) - p.rectDecoupe.w);
					nkgui::SliderFloat(ctx, "découpe : écart au bord", p.rectDecoupe.y, 0.f, 40.f);
					nkgui::SliderFloat(ctx, "marge de la découpe", p.margeDecoupe, 0.f, 90.f);
					nkgui::Checkbox(ctx, "même marge des deux côtés en paysage (iOS)", p.decoupeSymetrique);
				}
				nkgui::Separator(ctx);
				nkgui::SliderFloat(ctx, "barre d'état (pt)", p.barreEtat, 0.f, 60.f);
				nkgui::Checkbox(ctx, "barre d'état gardée en paysage", p.barreEtatPaysage);
				nkgui::SliderFloat(ctx, "indicateur, portrait", p.indicateur, 0.f, 60.f);
				nkgui::SliderFloat(ctx, "indicateur, paysage", p.indicateurPaysage, 0.f, 60.f);
				nkgui::SliderFloat(ctx, "navigation à 3 boutons", p.navBoutons, 0.f, 64.f);
				// Les marges suivent : elles sont DEDUITES, jamais tapees.
				p.orientation = NkOrientation::NK_PORTRAIT;
				p.zoneSure = NkMargesSysteme(p, NkOrientation::NK_PORTRAIT);
				ctx.PopId();
			}

		} // namespace

		void NkEditeurSectionAppareil(NkEditeurCadre &c) {
			NkGuiContext &ctx = c.ctx;
			NkEditeurModele &m = c.m;
			const NkProfilAppareil pa = m.ProfilCourant();
			const renderer::NkLayoutInfo px = NkLayoutSimule(pa);
			nkgui::Text(ctx, "Appareil simulé");
			ctx.PushId("appareil");
			nkgui::Text(ctx, NkString::Format("%s  %ux%u pt  (%ux%u px, x%.3g)  %s", pa.nom, pa.largeur, pa.hauteur, px.width,
											  px.height, static_cast<double>(pa.densite), NkNomSysteme(pa.systeme))
								 .CStr());
			const NkProfilAppareil base = m.ProfilDeBase();
			const bool naturelPaysage = base.largeur > base.hauteur;
			const char *noms[4] = {NkNomOrientation(NkOrientation::NK_PORTRAIT, naturelPaysage),
								   NkNomOrientation(NkOrientation::NK_PAYSAGE_GAUCHE, naturelPaysage),
								   NkNomOrientation(NkOrientation::NK_PORTRAIT_INVERSE, naturelPaysage),
								   NkNomOrientation(NkOrientation::NK_PAYSAGE_DROITE, naturelPaysage)};
			int32 o = static_cast<int32>(m.orientation);
			if (Choix(ctx, "orientation", noms, 4, o)) {
				m.orientation = static_cast<NkOrientation>(o);
			}
			if (pa.orientationNonProposee) {
				nkgui::TextWrapped(ctx, "Attention : le système ne tourne pas l'interface ainsi sur cet appareil ; "
										"les marges montrées sont celles de la rotation, à titre indicatif.");
			}
			nkgui::Checkbox(ctx, "cadre", m.appareil.voirCadre);
			ctx.SameLine();
			nkgui::Checkbox(ctx, "clair", m.appareil.cadreClair);
			ctx.SameLine();
			nkgui::Checkbox(ctx, "zone sûre", m.appareil.voirZoneSure);
			ctx.SameLine();
			nkgui::Checkbox(ctx, "découpe", m.appareil.voirDecoupe);
			nkgui::Checkbox(ctx, "aperçu de la caméra du jeu", m.appareil.apercuJeu);
			{
				static const char *kRegles[5] = {"tout", "hauteur", "largeur", "bandes", "remplir"};
				int32 regle = static_cast<int32>(m.appareil.regleCamera);
				if (Choix(ctx, "caméra du jeu", kRegles, 5, regle)) {
					m.appareil.regleCamera = static_cast<unkeny::NkRegleCamera>(regle);
				}
			}
			nkgui::Text(ctx, NkString::Format("zone sûre (pt)  h:%.0f b:%.0f g:%.0f d:%.0f", pa.zoneSure.top,
											  pa.zoneSure.bottom, pa.zoneSure.left, pa.zoneSure.right)
								 .CStr());
			nkgui::Text(ctx, NkString::Format("donnée au jeu (px)  h:%.0f b:%.0f g:%.0f d:%.0f", px.safeArea.top,
											  px.safeArea.bottom, px.safeArea.left, px.safeArea.right)
								 .CStr());
			if (pa.margeConseillee > 0.f) {
				nkgui::Text(ctx, NkString::Format("marge conseillée, non rendue par le système : %.1f %%",
												  static_cast<double>(pa.margeConseillee * 100.f))
									 .CStr());
			}
			if (pa.decoupe != NkTypeDecoupe::NK_AUCUNE) {
				nkgui::Text(ctx, NkString::Format("découpe : %s %.0fx%.0f pt", NkNomDecoupe(pa.decoupe),
												  static_cast<double>(pa.rectDecoupe.w), static_cast<double>(pa.rectDecoupe.h))
									 .CStr());
			}
			nkgui::TextWrapped(ctx, pa.provenance);
			if (m.ProfilPersonnalise()) {
				Personnalise(ctx, m);
			} else if (nkgui::Button(ctx, "Personnaliser cet appareil")) {
				m.appareil.persoBase = m.profil;
				m.appareil.perso = NkPersonnaliser(NkProfil(m.profil));
				m.profil = NkNbProfils();
			}
			ctx.PopId();
		}


		// =====================================================================
		// L'INTERFACE ANCREE A L'ECRAN (document 03, §2.5)
		// =====================================================================

		ecs::NkEntityId NkEditeurAjouterHud(NkEditeurModele &m, NkAncre ancre, const char *nom, uint32 couleur,
											const NkVec2f &taille) {
			const ecs::NkEntityId id = NkEditeurCreerEntite(m, nom, m.scene.Camera().Centre());
			if (!id.IsValid()) {
				return id;
			}
			NkSprite2D s;
			s.taille = taille;
			s.couleur = couleur;
			s.couche = 100; // devant le decor : c'est une interface
			m.scene.Monde().Add<NkSprite2D>(id, s);
			NkAncrageEcran2D a;
			a.ancre = static_cast<uint8>(ancre);
			m.scene.Monde().Add<NkAncrageEcran2D>(id, a);
			return id;
		}

		bool NkEditeurAncrerSelection(NkEditeurModele &m) {
			if (!m.aSelection || m.etat != NkEtatJeu::NK_EDITION || m.scene.Monde().Has<NkAncrageEcran2D>(m.selection)) {
				return false;
			}
			NkEditeurRetenir(m);
			m.scene.Monde().Add<NkAncrageEcran2D>(m.selection, NkAncrageEcran2D());
			return true;
		}

		void NkEditeurExempleHud(NkEditeurModele &m) {
			// Quatre elements, aux quatre coins de la ZONE SURE : un score, une
			// pause, un saut, une croix de direction. Le meme decor qu'a l'ouverture.
			NkEditeurAjouterHud(m, NkAncre::NK_HAUT_GAUCHE, "HUD Score", 0x2E3A5CF0u, NkVec2f(2.6f, 0.8f));
			NkEditeurAjouterHud(m, NkAncre::NK_HAUT_DROITE, "HUD Pause", 0xE0A030FFu, NkVec2f(0.9f, 0.9f));
			NkEditeurAjouterHud(m, NkAncre::NK_BAS_DROITE, "HUD Saut", 0x3C9AE0FFu, NkVec2f(1.4f, 1.4f));
			NkEditeurAjouterHud(m, NkAncre::NK_BAS_GAUCHE, "HUD Direction", 0x6E7C96D0u, NkVec2f(1.8f, 1.8f));
			m.aSelection = false;
		}

		// (2026-10-01, R33) NkEditeurBlocAncrage : devenu la carte « Ancrage a l'ecran »
		// des Details (NkEditeurDetails.cpp, CarteAncrage).

	} // namespace editeur
} // namespace nkentseu
