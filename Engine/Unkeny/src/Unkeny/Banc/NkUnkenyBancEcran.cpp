//
// NkUnkenyBancEcran.cpp
// =============================================================================
// Description :
//   Le banc de l'ECRAN DU JEU et de sa ZONE SURE (document 03 d'UnkenyEditor,
//   §2.5), lance par l'editeur ET par le joueur (`--selftest`) : c'est le meme
//   code des deux cotes, il s'eprouve des deux cotes.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (z1) le JOUEUR : un layout de NKCanvas (1179x2556 px, haut 177, bas 102)
//        donne l'ecran a l'origine, marges intactes, rectangle sur
//        {0,177,1179,2277}
//   (z2) l'EDITEUR : le meme layout affiche reduit au tiers dans le viseur
//        donne des marges et une densite au tiers, et le MEME rectangle sur
//        en proportion
//   (z3) des marges plus grandes que l'ecran : un rectangle sur VIDE, centre,
//        jamais a l'envers
//   (z4) NkAncrer : les neuf ancres, decalage vers l'INTERIEUR
//   (z5) NkAppliquerAncrages : un HUD ancre en haut a droite tombe au coin
//        haut droit de la zone sure (decalage x densite), dans le MONDE ; sans
//        ecran, rien ne bouge ; NkRendreAncrages rend la position
//   (z6) NkZoneSureMonde : les coins du rectangle sur, a travers la camera
//   (z7) l'ancrage s'enregistre et se relit avec la scene (JSON)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
#include "Unkeny/Banc/NkUnkenyBanc.h"
#include "Unkeny/Partie/NkUnkenyZoneSure.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			bool Proche(float32 a, float32 b) {
				return std::fabs(a - b) < 0.02f;
			}

			bool MemeRect(const nkgui::NkRect &r, float32 x, float32 y, float32 w, float32 h) {
				return Proche(r.x, x) && Proche(r.y, y) && Proche(r.w, w) && Proche(r.h, h);
			}

			renderer::NkLayoutInfo LayoutIlot() {
				renderer::NkLayoutInfo l;
				l.width = 1179u;
				l.height = 2556u;
				l.density = 3.f;
				l.safeArea = NkSafeAreaInsets(177.f, 102.f, 0.f, 0.f);
				return l;
			}
		} // namespace

		int32 NkUnkenyLancerBancEcran() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkeny — banc de l'ecran du jeu et de la zone sure\n\n");

			// (z1)
			{
				const NkEcranDuJeu e = NkEcranDepuisLayout(LayoutIlot(), nkgui::NkRect{0.f, 0.f, 0.f, 0.f}, false);
				Temoin(MemeRect(e.rect, 0.f, 0.f, 1179.f, 2556.f) && Proche(e.marges.top, 177.f) && Proche(e.densite, 3.f) &&
						   !e.simule && MemeRect(NkZoneSure(e), 0.f, 177.f, 1179.f, 2277.f),
					   "(z1) joueur : le layout de NKCanvas, zone sure 0,177 1179x2277", NkZoneSure(e).h);
			}

			// (z2)
			{
				const NkEcranDuJeu e = NkEcranDepuisLayout(LayoutIlot(), nkgui::NkRect{100.f, 50.f, 393.f, 852.f}, true);
				Temoin(Proche(e.marges.top, 59.f) && Proche(e.marges.bottom, 34.f) && Proche(e.densite, 1.f) && e.simule &&
						   MemeRect(NkZoneSure(e), 100.f, 109.f, 393.f, 759.f),
					   "(z2) editeur : le meme layout reduit au tiers, meme zone en proportion", e.marges.top);
			}

			// (z3)
			{
				NkEcranDuJeu e;
				e.rect = nkgui::NkRect{0.f, 0.f, 100.f, 50.f};
				e.marges = NkSafeAreaInsets(40.f, 40.f, 70.f, 70.f);
				const nkgui::NkRect z = NkZoneSure(e);
				Temoin(z.w == 0.f && z.h == 0.f && Proche(z.x, 50.f) && Proche(z.y, 25.f),
					   "(z3) marges trop grandes : rectangle sur vide, centre", z.x);
			}

			// (z4)
			{
				const nkgui::NkRect zone{10.f, 20.f, 200.f, 100.f};
				const nkgui::NkRect hg = NkAncrer(zone, NkAncre::NK_HAUT_GAUCHE, 20.f, 10.f, 4.f, 6.f);
				const nkgui::NkRect hd = NkAncrer(zone, NkAncre::NK_HAUT_DROITE, 20.f, 10.f, 4.f, 6.f);
				const nkgui::NkRect c = NkAncrer(zone, NkAncre::NK_CENTRE, 20.f, 10.f, 0.f, 0.f);
				const nkgui::NkRect bd = NkAncrer(zone, NkAncre::NK_BAS_DROITE, 20.f, 10.f, 4.f, 6.f);
				const nkgui::NkRect b = NkAncrer(zone, NkAncre::NK_BAS, 20.f, 10.f, 0.f, 6.f);
				Temoin(MemeRect(hg, 14.f, 26.f, 20.f, 10.f) && MemeRect(hd, 186.f, 26.f, 20.f, 10.f) &&
						   MemeRect(c, 100.f, 65.f, 20.f, 10.f) && MemeRect(bd, 186.f, 104.f, 20.f, 10.f) &&
						   MemeRect(b, 100.f, 104.f, 20.f, 10.f),
					   "(z4) NkAncrer : neuf ancres, decalage vers l'interieur", hd.x);
			}

			// (z5) (z6) (z7)
			{
				NkScene s;
				NkSceneConfig cfg;
				cfg.physique = false;
				s.Init(cfg);
				// Un telephone a ilot en paysage : 2556x1179, gauche et droite 177.
				renderer::NkLayoutInfo l;
				l.width = 2556u;
				l.height = 1179u;
				l.density = 3.f;
				l.safeArea = NkSafeAreaInsets(0.f, 63.f, 177.f, 177.f);
				const nkgui::NkRect fenetre{0.f, 0.f, 2556.f, 1179.f};
				s.Camera().PoserViseur(fenetre);
				s.Camera().PoserCentre(NkVec2f(0.f, 0.f));
				s.Camera().PoserZoom(100.f);
				const ecs::NkEntityId hud = s.Creer("HUD", NkVec2f(5.f, 5.f));
				NkSprite2D sp;
				sp.taille = NkVec2f(2.f, 1.f); // 200 x 100 px a ce zoom
				s.Monde().Add<NkSprite2D>(hud, sp);
				NkAncrageEcran2D a;
				a.ancre = static_cast<uint8>(NkAncre::NK_HAUT_DROITE);
				a.decalage = NkVec2f(10.f, 8.f); // points : x3 = 30 x 24 px
				s.Monde().Add<NkAncrageEcran2D>(hud, a);

				const int32 sansEcran = NkAppliquerAncrages(s);
				const NkVec2f apresSans = s.Monde().Get<NkTransform2D>(hud)->position;
				s.PoserEcran(NkEcranDepuisLayout(l, fenetre, false));
				NkVector<NkPositionAncree> avant;
				const int32 poses = NkAppliquerAncrages(s, &avant);
				const NkVec2f p = s.Monde().Get<NkTransform2D>(hud)->position;
				// Le coin haut droit de la zone sure : x = 2556 - 177 = 2379, y = 0.
				// Le sprite (200x100) recule de 30 px, descend de 24 : son centre
				// est a (2379 - 30 - 100, 24 + 50) = (2249, 74) a l'ecran.
				const NkVec2f attendu = s.Camera().EcranVersMonde(NkVec2f(2249.f, 74.f));
				const bool pose = poses == 1 && Proche(p.x, attendu.x) && Proche(p.y, attendu.y);
				NkRendreAncrages(s, avant);
				const NkVec2f rendu = s.Monde().Get<NkTransform2D>(hud)->position;
				Temoin(sansEcran == 0 && Proche(apresSans.x, 5.f) && pose && Proche(rendu.x, 5.f) && Proche(rendu.y, 5.f),
					   "(z5) HUD ancre haut droite : coin de la zone sure, rendu ensuite", p.x);

				// (z6)
				NkVec2f mn, mx;
				const bool z6 = NkZoneSureMonde(s, mn, mx);
				const NkVec2f hg = s.Camera().EcranVersMonde(NkVec2f(177.f, 0.f));
				const NkVec2f bd = s.Camera().EcranVersMonde(NkVec2f(2379.f, 1116.f));
				Temoin(z6 && Proche(mn.x, hg.x) && Proche(mx.y, hg.y) && Proche(mx.x, bd.x) && Proche(mn.y, bd.y),
					   "(z6) NkZoneSureMonde : les coins du rectangle sur dans le monde", mx.x - mn.x);

				// (z7)
				NkString json;
				const bool ecrit = NkSauverSceneJSON(s, json, static_cast<const NkTextures2D *>(nullptr));
				NkScene t;
				NkString err;
				const bool lu = ecrit && NkChargerSceneJSON(t, json.View(), static_cast<NkTextures2D *>(nullptr), &err);
				const NkAncrageEcran2D *ra = nullptr;
				t.Monde().Query<NkAncrageEcran2D>().ForEach([&ra](ecs::NkEntityId, NkAncrageEcran2D &x) { ra = &x; });
				Temoin(lu && ra != nullptr && ra->ancre == a.ancre && Proche(ra->decalage.x, 10.f) && Proche(ra->decalage.y, 8.f) &&
						   ra->zoneSure,
					   "(z7) l'ancrage s'enregistre et se relit avec la scene", ra != nullptr ? ra->decalage.x : -1.f);
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC ECRAN REUSSI" : "BANC ECRAN EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
