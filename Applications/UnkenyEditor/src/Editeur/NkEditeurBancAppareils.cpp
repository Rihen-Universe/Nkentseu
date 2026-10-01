//
// NkEditeurBancAppareils.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest` : les APPAREILS simules et leur zone sure
//   (document 03). Compte A PART (« BANC APPAREILS ») : les bancs d'avant
//   gardent leurs comptes.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (a1)  le catalogue : 21 profils, tous avec une provenance ; les six
//         d'origine gardent leurs marges de portrait (sauf l'encoche, dont les
//         chiffres etaient ceux de l'ilot : 47/34 desormais, l'ilot a son profil)
//   (a2)  iOS a encoche, portrait : haut 47, bas 34, cotes 0
//   (a3)  iOS a encoche, paysage GAUCHE et DROITE : gauche 47 ET droite 47,
//         haut 0, bas 21 (UIView.safeAreaInsets)
//   (a4)  Android a poinçon : la marge du SEUL cote de la decoupe -- a gauche
//         en paysage gauche, a droite en paysage droite ; barre d'etat en haut
//   (a5)  Android a trois boutons : la barre suit le bord PHYSIQUE bas (droite
//         en paysage gauche, gauche en paysage droite)
//   (a6)  la decoupe TOURNE geometriquement : collee au bord gauche en paysage
//         gauche, au bord droit en paysage droite, en bas en portrait inverse
//   (a7)  l'orientation est ABSOLUE : orienter un profil deja tourne revient au
//         meme que l'orienter depuis le portrait ; retour au portrait exact
//   (a8)  iPad en paysage garde barre d'etat et indicateur (haut 24, bas 20) ;
//         dimensions echangees, rayon des coins inchange
//   (a9)  NkTourner (compatibilite) = le paysage gauche de NkOrienter
//   (a10) portrait inverse refuse sur iPhone et telephone Android, propose
//         sur iPad
//   (a11) NkLayoutSimule rend la structure de NKCanvas : pixels physiques
//         (ilot : 1179x2556, haut 177, bas 102, densite 3)
//   (a12) TV et montre ronde : AUCUNE marge rendue par le systeme, une marge
//         CONSEILLEE a part
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurAppareils.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			bool Proche(float32 a, float32 b) {
				return std::fabs(a - b) < 0.01f;
			}

			/// Les quatre marges, dans l'ordre haut, bas, gauche, droite.
			bool Marges(const NkSafeAreaInsets &z, float32 t, float32 b, float32 l, float32 r) {
				return Proche(z.top, t) && Proche(z.bottom, b) && Proche(z.left, l) && Proche(z.right, r);
			}

			bool MemeRect(const NkRectAppareil &a, const NkRectAppareil &b) {
				return Proche(a.x, b.x) && Proche(a.y, b.y) && Proche(a.w, b.w) && Proche(a.h, b.h);
			}

			constexpr NkOrientation kP = NkOrientation::NK_PORTRAIT;
			constexpr NkOrientation kG = NkOrientation::NK_PAYSAGE_GAUCHE;
			constexpr NkOrientation kI = NkOrientation::NK_PORTRAIT_INVERSE;
			constexpr NkOrientation kD = NkOrientation::NK_PAYSAGE_DROITE;
		} // namespace

		int32 NkEditeurLancerBancAppareils() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyEditor — banc des appareils simules et de la zone sure\n\n");

			// (a1)
			{
				bool provenances = true;
				for (int32 i = 0; i < NkNbProfils(); ++i) {
					const NkProfilAppareil p = NkProfil(i);
					provenances = provenances && p.provenance != nullptr && p.provenance[0] != '\0' && p.largeur > 0u &&
								  p.hauteur > 0u && p.densite > 0.f;
				}
				const bool origine = Marges(NkProfil(0).zoneSure, 0.f, 0.f, 0.f, 0.f) &&
									 Marges(NkProfil(2).zoneSure, 24.f, 24.f, 0.f, 0.f) &&
									 Marges(NkProfil(3).zoneSure, 24.f, 0.f, 0.f, 0.f) &&
									 Marges(NkProfil(4).zoneSure, 24.f, 20.f, 0.f, 0.f) &&
									 Marges(NkProfil(5).zoneSure, 0.f, 0.f, 0.f, 0.f) &&
									 Marges(NkProfil(6).zoneSure, 59.f, 34.f, 0.f, 0.f);
				Temoin(NkNbProfils() == 21 && provenances && origine,
					   "(a1) catalogue : 21 profils sources ; marges d'origine gardees", static_cast<float32>(NkNbProfils()));
			}

			const NkProfilAppareil encoche = NkProfil(1);
			// (a2)
			Temoin(Marges(encoche.zoneSure, 47.f, 34.f, 0.f, 0.f) && encoche.decoupe == NkTypeDecoupe::NK_ENCOCHE,
				   "(a2) iOS a encoche, portrait : haut 47, bas 34", encoche.zoneSure.top);

			// (a3)
			{
				const NkProfilAppareil g = NkOrienter(encoche, kG);
				const NkProfilAppareil d = NkOrienter(encoche, kD);
				Temoin(Marges(g.zoneSure, 0.f, 21.f, 47.f, 47.f) && Marges(d.zoneSure, 0.f, 21.f, 47.f, 47.f),
					   "(a3) iOS paysage G et D : gauche 47 ET droite 47, haut 0, bas 21", g.zoneSure.right);
			}

			// (a4)
			{
				const NkProfilAppareil a = NkProfil(2);
				const NkProfilAppareil g = NkOrienter(a, kG);
				const NkProfilAppareil d = NkOrienter(a, kD);
				Temoin(Marges(g.zoneSure, 24.f, 24.f, 24.f, 0.f) && Marges(d.zoneSure, 24.f, 24.f, 0.f, 24.f),
					   "(a4) Android poinçon : marge du SEUL cote de la decoupe", g.zoneSure.left - g.zoneSure.right);
			}

			// (a5)
			{
				const NkProfilAppareil a = NkProfil(8);
				const NkProfilAppareil g = NkOrienter(a, kG);
				const NkProfilAppareil d = NkOrienter(a, kD);
				Temoin(Marges(a.zoneSure, 24.f, 48.f, 0.f, 0.f) && Marges(g.zoneSure, 24.f, 0.f, 24.f, 48.f) &&
						   Marges(d.zoneSure, 24.f, 0.f, 48.f, 24.f),
					   "(a5) Android 3 boutons : la barre suit le bord physique bas", g.zoneSure.right);
			}

			// (a6)
			{
				const NkProfilAppareil g = NkOrienter(encoche, kG);
				const NkProfilAppareil d = NkOrienter(encoche, kD);
				const NkProfilAppareil i = NkOrienter(encoche, kI);
				const bool gauche = MemeRect(g.rectDecoupe, NkRectAppareil{0.f, 114.f, 34.f, 162.f});
				const bool droite = MemeRect(d.rectDecoupe, NkRectAppareil{810.f, 114.f, 34.f, 162.f});
				const bool bas = MemeRect(i.rectDecoupe, NkRectAppareil{114.f, 810.f, 162.f, 34.f});
				Temoin(gauche && droite && bas, "(a6) la decoupe tourne : bord gauche, bord droit, bord bas", d.rectDecoupe.x);
			}

			// (a7)
			{
				const NkProfilAppareil ilot = NkProfil(6);
				const NkProfilAppareil viaG = NkOrienter(NkOrienter(ilot, kG), kD);
				const NkProfilAppareil direct = NkOrienter(ilot, kD);
				const NkProfilAppareil retour = NkOrienter(NkOrienter(ilot, kI), kP);
				const bool absolu = MemeRect(viaG.rectDecoupe, direct.rectDecoupe) && viaG.largeur == direct.largeur &&
									Marges(viaG.zoneSure, direct.zoneSure.top, direct.zoneSure.bottom, direct.zoneSure.left,
										   direct.zoneSure.right);
				const bool exact = MemeRect(retour.rectDecoupe, ilot.rectDecoupe) && retour.largeur == ilot.largeur &&
								   retour.hauteur == ilot.hauteur && Marges(retour.zoneSure, 59.f, 34.f, 0.f, 0.f);
				Temoin(absolu && exact, "(a7) orientation absolue ; retour au portrait exact", retour.rectDecoupe.x);
			}

			// (a8)
			{
				const NkProfilAppareil t = NkProfil(4);
				const NkProfilAppareil g = NkOrienter(t, kG);
				Temoin(Marges(g.zoneSure, 24.f, 20.f, 0.f, 0.f) && g.largeur == t.hauteur && g.hauteur == t.largeur &&
						   Proche(g.rayonCoins, t.rayonCoins),
					   "(a8) iPad paysage : haut 24, bas 20 ; dimensions echangees", g.zoneSure.top);
			}

			// (a9)
			{
				const NkProfilAppareil t = NkTourner(encoche);
				const NkProfilAppareil g = NkOrienter(encoche, kG);
				Temoin(t.orientation == kG && MemeRect(t.rectDecoupe, g.rectDecoupe) &&
						   Marges(t.zoneSure, g.zoneSure.top, g.zoneSure.bottom, g.zoneSure.left, g.zoneSure.right) &&
						   t.zoneSure.right > 0.f,
					   "(a9) NkTourner = paysage gauche (droite non nulle sur iOS)", t.zoneSure.right);
			}

			// (a10)
			Temoin(NkOrienter(encoche, kI).orientationNonProposee && NkOrienter(NkProfil(2), kI).orientationNonProposee &&
					   !NkOrienter(NkProfil(4), kI).orientationNonProposee && !NkOrienter(encoche, kG).orientationNonProposee,
				   "(a10) portrait inverse : refuse sur telephone, propose sur iPad", 0.f);

			// (a11)
			{
				const renderer::NkLayoutInfo l = NkLayoutSimule(NkProfil(6));
				const renderer::NkLayoutInfo lg = NkLayoutSimule(NkOrienter(NkProfil(6), kG));
				Temoin(l.width == 1179u && l.height == 2556u && Proche(l.density, 3.f) && Proche(l.safeArea.top, 177.f) &&
						   Proche(l.safeArea.bottom, 102.f) && lg.width == 2556u && Proche(lg.safeArea.left, 177.f) &&
						   Proche(lg.safeArea.right, 177.f) && !lg.IsPortrait(),
					   "(a11) NkLayoutSimule : pixels physiques, comme NKCanvas", l.safeArea.top);
			}

			// (a12)
			{
				const NkProfilAppareil tv = NkProfil(17);
				const NkProfilAppareil montre = NkProfil(20);
				Temoin(tv.zoneSure.IsZero() && montre.zoneSure.IsZero() && tv.margeConseillee > 0.f && montre.rond &&
						   montre.margeConseillee > 0.14f,
					   "(a12) TV, montre : aucune marge systeme, une marge conseillee", montre.margeConseillee);
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC APPAREILS REUSSI" : "BANC APPAREILS EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
