// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurViseur.cpp
// DESCRIPTION: Le viseur de l'editeur. Il dessine ; il ne modifie rien.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurViseur.h"
#include "Editeur/NkEditeurActions.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;

		nkgui::NkRect NkAireAppareil(const nkgui::NkRect &viseur, const NkProfilAppareil &profil) noexcept {
			const float32 rapport = static_cast<float32>(profil.largeur) / static_cast<float32>(profil.hauteur);
			const float32 marge = 24.f;
			float32 aw = viseur.w - marge * 2.f;
			float32 ah = aw / rapport;
			if (ah > viseur.h - marge * 2.f) {
				ah = viseur.h - marge * 2.f;
				aw = ah * rapport;
			}
			// ⚠️ Un panneau peut etre reduit a presque rien par l'ancrage. Sans
			// ce plancher, l'aire devient negative, la camera recoit un viseur
			// vide, et la division par sa largeur rend des coordonnees infinies
			// -- une panne qui sort loin d'ici, dans la conversion ecran/monde.
			if (aw < 1.f) {
				aw = 1.f;
			}
			if (ah < 1.f) {
				ah = 1.f;
			}
			return nkgui::NkRect{viseur.x + (viseur.w - aw) * 0.5f, viseur.y + (viseur.h - ah) * 0.5f, aw, ah};
		}

		NkStatsRendu NkDessinerViseur(nkgui::NkGuiDrawList &dl, NkEditeurModele &m, const nkgui::NkRect &viseur,
									  const nkgui::NkRect &appareil) {
			NkScene &scene = m.scene;
			const NkTheme &th = m.theme;
			const NkProfilAppareil profil = m.ProfilCourant();
			NkStatsRendu stats;
			// Le fond du viseur, puis l'aire d'appareil : deux tons distincts,
			// pour qu'on voie du premier coup d'oeil ce qui est DANS l'ecran
			// simule et ce qui est autour.
			dl.AddRectFilled(viseur, NkColor(10, 12, 17));
			dl.AddRectFilled(appareil, NkColor(20, 23, 31));
			// ⚠️ DECOUPE OBLIGATOIRE sur l'aire d'appareil. Sans elle, une scene
			// plus grande que l'ecran simule deborde sur les panneaux — et on
			// juge une mise en page mobile sur une image qui montre plus que ce
			// que le telephone montrerait.
			dl.PushClipRect(appareil, true);
			if (m.voirGrille) {
				NkDessinerGrille(dl, scene.Camera(), 1.f);
			}
			// L'ordre : les FORMES (decor, rigides sans texture), puis les sprites,
			// puis la MATIERE par-dessus — elle coule sur tout le reste.
			NkOptionsFormes formes;
			formes.couleurDecor = [](ecs::NkWorld &w, ecs::NkEntityId id, void *) -> uint32 {
				const NkSprite2D *s = w.Get<NkSprite2D>(id);
				return s != nullptr ? s->couleur : 0u; // le sprite (cache) garde la couleur du decor
			};
			NkDessinerFormes(dl, scene, formes);
			stats = NkDessinerScene(dl, scene);
			NkDessinerCorpsMous(dl, scene, m.rendu);
			if (m.voirCollisionneurs) {
				NkDessinerCollisionneurs(dl, scene, 0x00E07AC0u);
			}
			// La selection : un cadre d'accent autour de sa boite, et son nom.
			NkVec2f mn, mx;
			if (NkEditeurBoiteSelection(m, mn, mx)) {
				const NkVec2f hg = scene.Camera().MondeVersEcran(NkVec2f(mn.x - 0.05f, mx.y + 0.05f));
				const NkVec2f bd = scene.Camera().MondeVersEcran(NkVec2f(mx.x + 0.05f, mn.y - 0.05f));
				dl.AddRect(NkRect{hg.x, hg.y, bd.x - hg.x, bd.y - hg.y}, th.or_, 2.f, 3.f);
				NkVec2f c;
				if (NkEditeurCentreSelection(m, c)) {
					// Une croix au centre : elle dit ou est l'ORIGINE de l'entite.
					const NkVec2f e = scene.Camera().MondeVersEcran(c);
					dl.AddLine(NkVec2f(e.x - 6.f, e.y), NkVec2f(e.x + 6.f, e.y), th.or_, 1.5f);
					dl.AddLine(NkVec2f(e.x, e.y - 6.f), NkVec2f(e.x, e.y + 6.f), th.or_, 1.5f);
				}
			}
			dl.PopClipRect();

			// --- LA ZONE SURE SIMULEE ------------------------------------
			// Elle est dessinee APRES la decoupe : c'est une surcouche de
			// l'editeur, pas un element de la scene.
			//
			// ⚠️ C'est la raison d'etre de tout ce panneau. Ce qui tombe dans les
			// bandes hachurees est INATTEIGNABLE sur l'appareil — pas mal place :
			// inatteignable. Et cela ne se voit JAMAIS depuis une machine de
			// bureau.
			const float32 ex = appareil.w / static_cast<float32>(profil.largeur);
			const float32 ey = appareil.h / static_cast<float32>(profil.hauteur);
			const NkColor voile(220, 90, 90, 46);
			const NkColor trait(235, 120, 120, 190);

			auto bande = [&](const NkRect &r) {
				if (r.w <= 0.f || r.h <= 0.f) {
					return;
				}
				dl.AddRectFilled(r, voile);
			};
			const float32 hHaut = profil.zoneSure.top * ey;
			const float32 hBas = profil.zoneSure.bottom * ey;
			const float32 lG = profil.zoneSure.left * ex;
			const float32 lD = profil.zoneSure.right * ex;
			bande(NkRect{appareil.x, appareil.y, appareil.w, hHaut});
			bande(NkRect{appareil.x, appareil.y + appareil.h - hBas, appareil.w, hBas});
			bande(NkRect{appareil.x, appareil.y, lG, appareil.h});
			bande(NkRect{appareil.x + appareil.w - lD, appareil.y, lD, appareil.h});

			// Le cadre de la zone SURE elle-meme : c'est dedans qu'un bouton doit
			// tenir.
			if (hHaut > 0.f || hBas > 0.f || lG > 0.f || lD > 0.f) {
				dl.AddRect(NkRect{appareil.x + lG, appareil.y + hHaut, appareil.w - lG - lD,
								  appareil.h - hHaut - hBas},
						   trait, 1.f);
			}
			// Le bord de l'appareil, toujours visible — VERT en jeu, AMBRE en
			// pause : on sait d'un regard si ce qu'on voit est la scene editee
			// ou un instant de simulation.
			const NkColor bordEtat = m.etat == NkEtatJeu::NK_JEU ? th.succes : (m.etat == NkEtatJeu::NK_PAUSE ? th.or_ : th.bord);
			dl.AddRect(appareil, bordEtat, 2.f, 6.f);
			return stats;
		}

		// =====================================================================
		bool NkEntiteSous(NkScene &scene, const NkVec2f &monde, ecs::NkEntityId &sortie, NkVec2f &centre) {
			bool trouve = false;
			ecs::NkEntityId candidat;
			NkVec2f c(0.f, 0.f);
			int32 meilleureCouche = -1000000;

			scene.Monde().Query<NkTransform2D, NkSprite2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkSprite2D &s) {
					if (!s.visible) {
						return;
					}
					const float32 hw = s.taille.x * math::NkAbs(t.echelle.x) * 0.5f;
					const float32 hh = s.taille.y * math::NkAbs(t.echelle.y) * 0.5f;
					if (monde.x < t.position.x - hw || monde.x > t.position.x + hw || monde.y < t.position.y - hh ||
						monde.y > t.position.y + hh) {
						return;
					}
					// ⚠️ On garde la couche la PLUS HAUTE : c'est celle qui est
					// dessinee au-dessus, donc celle que l'utilisateur voit et
					// croit cliquer. Garder la premiere trouvee selectionnerait
					// ce qui est CACHE.
					if (s.couche >= meilleureCouche) {
						meilleureCouche = s.couche;
						candidat = id;
						c = t.position;
						trouve = true;
					}
				});

			if (trouve) {
				sortie = candidat;
				centre = c;
			}
			return trouve;
		}

	} // namespace editeur
} // namespace nkentseu
