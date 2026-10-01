//
// NkEditeurBancFormes.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest` : les gestes de l'editeur sur les FORMES 2D, le
//   panneau « Placer des acteurs » et la COLLISION (menus, Details, poignees).
//   Compte A PART (« BANC FORMES EDITEUR ») : les bancs d'avant gardent leurs
//   comptes. Les gestes passent par la VRAIE trame (NkEditeurDessinerTrame),
//   la souris par NkEditeurSouris comme depuis l'OS -- jamais la vraie souris.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (p1)  le catalogue : Base, Lumieres, Formes (9 genres + carre), Effets,
//         Volumes ont des elements ; tout element est dans « Tout »
//   (p2)  « + Ajouter > Formes 2D » : chaque genre pose, avec sa forme, son
//         collisionneur assorti et un corps STATIQUE ; Ctrl+Z retire le dernier
//   (p3)  « Ajouter ici » : la forme nait au point du clic droit
//   (p4)  la disposition : le panneau a GAUCHE, l'Outliner a sa droite, le
//         viseur apres ; panneau ferme, l'Outliner revient a x = 0
//   (p5)  le panneau dans la vraie trame : l'onglet Formes se clique ; un CLIC
//         sur « Etoile » la pose au centre de la vue
//   (p6)  GLISSER « Cercle » du panneau vers un point de la vue : le cercle nait
//         A CE POINT (au centimetre) ; (p6n) lache sur l'Outliner : rien
//   (p7)  la recherche « eto » ne montre que l'etoile ; l'etoile d'une ligne la
//         met dans les Favoris
//   (p8)  un VOLUME glisse SUR une entite devient son collisionneur (pas
//         d'entite de plus)
//   (p9)  « Ajouter un composant > Collisionneur » : depuis le contour du sprite
//         (la caisse texturee), polygone, depuis la forme (etoile de decor :
//         une chaine)
//   (p10) les POIGNEES : un coin de boite tire elargit la boite ; la poignee de
//         rotation tourne le collisionneur ; le « + » d'un polygone ajoute un
//         sommet, Ctrl+clic en retire un ; un corps rigide suit
//   (p11) une forme choisie : la trame dessine Details (bloc Forme et
//         Collision) et la vue (poignees allumees) sans rien casser
//   (p12) Enregistrer puis Ouvrir : les formes, leurs genres, et la matrice
//         des calques reviennent
//   (p13) Jouer : une etoile DYNAMIQUE tombe sur un rectangle et s'y pose ;
//         Arreter la rend a sa place
//   L'AIMANT (demande de Rihen du 01/10, NkEditeurAimant.cpp) :
//   (a1)  eteint par defaut : la cible passe telle quelle
//   (a2)  sommet : un coin de B a 8 cm d'un coin de A1 s'y pose EXACTEMENT
//   (a3)  arete : loin des coins, B se pose sur le dessus de A1, x garde
//   (a4)  face : B enfonce dans A2 au-dela du rayon ressort sur sa surface
//   (a5)  priorite sommet > arete : l'arete plus proche ne gagne pas
//   (a6)  une poignee tiree pres d'un coin s'y pose
//   (a7)  V maintenue l'allume ; (a8) le bouton de la barre flottante aussi
//   (a9)  dans la vraie trame, B glisse a la souris et colle coin contre coin
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurSouris.h"
#include "Editeur/NkEditeurTrame.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			/// LA VRAIE TRAME, sans fenetre (meme montage que le banc des actions).
			struct NkTrame {
					memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
					NkEditeurModele &m;
					NkEditeurInterface *pui;
					nkgui::NkGuiContext *pctx;
					nkgui::NkGuiFont *police;
					NkEditeurEntrees *pent;
					NkEditeurConstruction *pcons;
					editorkit::NkTheme theme = editorkit::NkTheme::Dark();
					NkPaletteEditeur pal;
					NkEditeurSouris souris;
					float32 W = 1400.f, H = 800.f;

					explicit NkTrame(NkEditeurModele &modele) : m(modele) {
						pui = alloc.New<NkEditeurInterface>();
						pctx = alloc.New<nkgui::NkGuiContext>();
						police = alloc.New<nkgui::NkGuiFont>();
						pent = alloc.New<NkEditeurEntrees>();
						pcons = alloc.New<NkEditeurConstruction>();
						NkEditeurEntreesParDefaut(*pent);
						const bool ok = police->LoadEmbedded(NkEmbeddedFontId::DroidSans, 13.f, false);
						pctx->Init(static_cast<int32>(W), static_cast<int32>(H));
						pctx->font = ok ? police : nullptr;
						pal = NkEditeurPalette(theme);
						pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
					}
					~NkTrame() {
						alloc.Delete(pcons);
						alloc.Delete(pent);
						alloc.Delete(police);
						alloc.Delete(pctx);
						alloc.Delete(pui);
					}
					NkEditeurInterface &Ui() {
						return *pui;
					}
					NkEditeurCadre Cadre() {
						return NkEditeurCadre{*pctx, m, *pui, theme, pal, police, police};
					}
					void Trame() {
						pui->dt = 1.f / 60.f;
						NkEditeurPlanifier(*pui, W, H);
						pctx->BeginFrame(1.f / 60.f);
						pctx->BeginLayout(nkgui::NkRect{0.f, 0.f, W, H});
						NkEditeurCadre c = Cadre();
						NkEditeurDessinerTrame(c, *pent, *pcons, nullptr);
						pctx->EndFrame();
						souris.FinDeTrame(pctx->input);
					}
					void Aller(float32 x, float32 y) {
						pctx->input.mousePos = nkgui::NkVec2{x, y};
						Trame();
					}
					void Appui(float32 x, float32 y) {
						pctx->input.mousePos = nkgui::NkVec2{x, y};
						souris.Appui(pctx->input, 0);
						Trame();
					}
					void Relache(float32 x, float32 y) {
						pctx->input.mousePos = nkgui::NkVec2{x, y};
						souris.Relache(pctx->input, 0);
						Trame();
					}
					void Clic(float32 x, float32 y) {
						Appui(x, y);
						Relache(x, y);
					}
					void Ctrl(bool v) {
						pctx->input.ctrlDown = v;
					}
					/// Un GLISSER, en quelques pas (le seuil du glisser est franchi).
					void Glisser(float32 x0, float32 y0, float32 x1, float32 y1) {
						Appui(x0, y0);
						for (int32 k = 1; k <= 6; ++k) {
							const float32 t = static_cast<float32>(k) / 6.f;
							Aller(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
						}
						Relache(x1, y1);
					}
			};

			uint32 Compter(NkScene &s) {
				uint32 n = 0;
				s.Monde().Query<NkRenduForme2D>().ForEach([&n](ecs::NkEntityId, NkRenduForme2D &f) {
					n += f.visible ? 1u : 0u;
				});
				return n;
			}

			uint32 NbEntites(NkScene &s) {
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				return static_cast<uint32>(ids.Size());
			}

			int32 Element(NkNaturePlacer nature, int32 indice) {
				int32 n = 0;
				const NkElementPlacer *cat = NkEditeurCataloguePlacer(n);
				for (int32 k = 0; k < n; ++k) {
					if (cat[k].nature == nature && cat[k].indice == indice) {
						return k;
					}
				}
				return -1;
			}

			nkgui::NkVec2 Centre(const nkgui::NkRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}

			NkVec2f Ecran(NkScene &s, const NkVec2f &monde) {
				return s.Camera().MondeVersEcran(monde);
			}
		} // namespace

		int32 NkEditeurLancerBancFormes() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkenyEditor — banc des formes 2D, de Placer des acteurs et de la collision\n\n");
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			NkTrame *pt = memory::NkGetDefaultAllocator().New<NkTrame>(m);
			NkTrame &T = *pt;
			NkEditeurInterface &ui = T.Ui();

			// (p1) le catalogue
			{
				int32 n = 0;
				const NkElementPlacer *cat = NkEditeurCataloguePlacer(n);
				int32 parOnglet[8] = {};
				bool tousDansTout = true;
				for (int32 k = 0; k < n; ++k) {
					for (int32 o = 0; o < 8; ++o) {
						parOnglet[o] += (cat[k].onglets & (1u << o)) != 0u ? 1 : 0;
					}
					tousDansTout = tousDansTout && (cat[k].onglets & (1u << static_cast<uint32>(NkOngletPlacer::NK_TOUT))) != 0u;
				}
				bool genres = true;
				for (int32 g = 0; g < static_cast<int32>(NkGenreForme2D::NK_COUNT); ++g) {
					genres = genres && Element(NkNaturePlacer::NK_FORME, g) >= 0;
				}
				Temoin(parOnglet[2] >= 6 && parOnglet[3] == 3 && parOnglet[4] == 10 && parOnglet[5] >= 6 && parOnglet[6] >= 5 && genres &&
						   tousDansTout,
					   "(p1) catalogue : Base, Lumieres 3, Formes 10, Effets, Volumes ; tout dans Tout", static_cast<float32>(n));
			}

			// (p2) « + Ajouter > Formes 2D »
			{
				NkEditeurCadre c = T.Cadre();
				const uint32 avant = Compter(m.scene);
				bool tous = true;
				ecs::NkEntityId derniere;
				for (int32 g = 0; g < static_cast<int32>(NkGenreForme2D::NK_COUNT); ++g) {
					NkEditeurExecuter(c, NK_A_FORME + g);
					const ecs::NkEntityId e = m.selection;
					const NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(e);
					const NkCorps2D *b = m.scene.Monde().Get<NkCorps2D>(e);
					tous = tous && m.aSelection && f != nullptr && static_cast<int32>(f->genre) == g &&
						   m.scene.Monde().Has<NkCollisionneur2D>(e) && b != nullptr && b->type == NkTypeCorps::NK_STATIQUE;
					derniere = e;
				}
				const uint32 apres = Compter(m.scene);
				NkEditeurExecuter(c, NK_A_ANNULER);
				const bool annule = Compter(m.scene) == apres - 1u && !m.scene.Monde().IsAlive(derniere);
				Temoin(tous && apres == avant + 9u && annule, "(p2) + Ajouter > Formes 2D : 9 genres, collision, corps ; Ctrl+Z",
					   static_cast<float32>(apres - avant));
			}

			// (p3) « Ajouter ici »
			{
				NkEditeurCadre c = T.Cadre();
				ui.pointContexte = NkVec2f(3.f, 2.f);
				NkEditeurExecuter(c, NK_A_FORME_ICI + static_cast<int32>(NkGenreForme2D::NK_TRIANGLE));
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(m.selection);
				const bool ok = t != nullptr && math::NkAbs(t->position.x - 3.f) < 1.0e-4f && math::NkAbs(t->position.y - 2.f) < 1.0e-4f &&
								m.scene.Monde().Has<NkRenduForme2D>(m.selection);
				Temoin(ok, "(p3) Ajouter ici > Triangle : au point du clic droit", 0.f);
			}

			// (p4) la disposition
			{
				NkEditeurPlanifier(ui, T.W, T.H);
				const bool ouvert = ui.placer.x == 0.f && ui.placer.w > 150.f && ui.outliner.x > ui.placer.x + ui.placer.w &&
									ui.viseur.x > ui.outliner.x + ui.outliner.w;
				ui.voirPlacer = false;
				NkEditeurPlanifier(ui, T.W, T.H);
				const bool ferme = ui.outliner.x == 0.f && ui.placer.w == 0.f;
				ui.voirPlacer = true;
				NkEditeurPlanifier(ui, T.W, T.H);
				Temoin(ouvert && ferme, "(p4) disposition : Placer a gauche, Outliner apres ; ferme, Outliner a 0", ui.placer.w);
			}

			// (p5) le panneau dans la vraie trame
			T.Trame();
			T.Trame();
			{
				const nkgui::NkVec2 o = Centre(ui.placerOngletsRects[static_cast<int32>(NkOngletPlacer::NK_FORMES)]);
				T.Clic(o.x, o.y);
				T.Trame();
				const int32 k = Element(NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_ETOILE));
				const bool visible = k >= 0 && ui.placerRects.Size() > static_cast<usize>(k) && ui.placerRects[static_cast<uint32>(k)].w > 0.f;
				const uint32 avant = Compter(m.scene);
				const NkVec2f centre = m.scene.Camera().Centre();
				if (visible) {
					const nkgui::NkVec2 p = Centre(ui.placerRects[static_cast<uint32>(k)]);
					T.Clic(p.x - 20.f, p.y); // pas sur l'etoile des favoris, a droite
				}
				const NkRenduForme2D *f = m.aSelection ? m.scene.Monde().Get<NkRenduForme2D>(m.selection) : nullptr;
				const NkTransform2D *t = m.aSelection ? m.scene.Monde().Get<NkTransform2D>(m.selection) : nullptr;
				const bool pose = f != nullptr && f->genre == NkGenreForme2D::NK_ETOILE && t != nullptr &&
								  math::NkAbs(t->position.x - centre.x) < 1.0e-3f && math::NkAbs(t->position.y - centre.y) < 1.0e-3f;
				Temoin(ui.placerOnglet == static_cast<int32>(NkOngletPlacer::NK_FORMES) && visible && pose && Compter(m.scene) == avant + 1u,
					   "(p5) onglet Formes clique ; clic sur Etoile : posee au centre de la vue", static_cast<float32>(ui.placerOnglet));
			}

			// (p6) le GLISSER vers la vue
			{
				const int32 k = Element(NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_CERCLE));
				const nkgui::NkVec2 depart = Centre(ui.placerRects[static_cast<uint32>(k)]);
				// Un point de la vue, loin de tout ce qui est deja pose.
				const nkgui::NkRect v = ui.viseur;
				const float32 x1 = v.x + v.w * 0.82f, y1 = v.y + v.h * 0.22f;
				const NkVec2f attendu = m.scene.Camera().EcranVersMonde(NkVec2f(x1, y1));
				const uint32 avant = Compter(m.scene);
				T.Glisser(depart.x - 20.f, depart.y, x1, y1);
				const NkTransform2D *t = m.aSelection ? m.scene.Monde().Get<NkTransform2D>(m.selection) : nullptr;
				const NkRenduForme2D *f = m.aSelection ? m.scene.Monde().Get<NkRenduForme2D>(m.selection) : nullptr;
				const float32 ecart = t != nullptr ? math::NkAbs(t->position.x - attendu.x) + math::NkAbs(t->position.y - attendu.y) : 99.f;
				Temoin(f != nullptr && f->genre == NkGenreForme2D::NK_CERCLE && ecart < 0.01f && Compter(m.scene) == avant + 1u,
					   "(p6) glisser Cercle du panneau vers la vue : pose au point lache (ecart m)", ecart);
				// (p6n) lache sur l'Outliner : rien.
				const uint32 avant2 = Compter(m.scene);
				const nkgui::NkVec2 o = Centre(ui.outliner);
				T.Glisser(depart.x - 20.f, depart.y, o.x, o.y);
				Temoin(Compter(m.scene) == avant2, "(p6n) lache sur l'Outliner : rien n'est pose", 0.f);
			}

			// (p7) la recherche et les favoris
			{
				std::snprintf(ui.placerFiltre, sizeof(ui.placerFiltre), "%s", "eto");
				T.Trame();
				int32 visibles = 0;
				const int32 etoile = Element(NkNaturePlacer::NK_FORME, static_cast<int32>(NkGenreForme2D::NK_ETOILE));
				for (uint32 k = 0; k < ui.placerRects.Size(); ++k) {
					visibles += ui.placerRects[k].w > 0.f ? 1 : 0;
				}
				const bool seule = visibles == 1 && ui.placerRects[static_cast<uint32>(etoile)].w > 0.f;
				// L'etoile des Favoris, a droite de la ligne.
				const nkgui::NkRect r = ui.placerRects[static_cast<uint32>(etoile)];
				T.Aller(r.x + r.w - 12.f, r.y + r.h * 0.5f);
				T.Clic(r.x + r.w - 12.f, r.y + r.h * 0.5f);
				ui.placerFiltre[0] = '\0';
				const bool favori = (ui.placerFavoris & (1ull << static_cast<uint64>(etoile))) != 0u;
				const nkgui::NkVec2 o = Centre(ui.placerOngletsRects[static_cast<int32>(NkOngletPlacer::NK_FAVORIS)]);
				T.Clic(o.x, o.y);
				T.Trame();
				const bool dansFavoris = ui.placerRects[static_cast<uint32>(etoile)].w > 0.f;
				Temoin(seule && favori && dansFavoris, "(p7) recherche « eto » : l'etoile seule ; etoile -> Favoris", static_cast<float32>(visibles));
			}

			// (p8) un volume glisse SUR une entite
			{
				// Un rectangle de decor, au milieu de la vue, sans autre chose dessus.
				const nkgui::NkRect v = ui.viseur;
				const NkVec2f cible = m.scene.Camera().EcranVersMonde(NkVec2f(v.x + v.w * 0.3f, v.y + v.h * 0.2f));
				const ecs::NkEntityId rect = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, cible);
				const nkgui::NkVec2 o = Centre(ui.placerOngletsRects[static_cast<int32>(NkOngletPlacer::NK_VOLUMES)]);
				T.Clic(o.x, o.y);
				T.Trame();
				const int32 k = Element(NkNaturePlacer::NK_VOLUME, static_cast<int32>(NkCollisionEditeur::NK_CERCLE));
				const nkgui::NkVec2 depart = Centre(ui.placerRects[static_cast<uint32>(k)]);
				const NkVec2f e = Ecran(m.scene, cible);
				const uint32 avant = NbEntites(m.scene);
				T.Glisser(depart.x - 20.f, depart.y, e.x, e.y);
				const NkCollisionneur2D *col = m.scene.Monde().Get<NkCollisionneur2D>(rect);
				if (!(col != nullptr && col->forme == NkForme2D::NK_CERCLE && NbEntites(m.scene) == avant && m.selection == rect)) {
					std::printf("      (p8) forme %d, entites %u -> %u, onglet %d, element %d (%.0f %.0f %.0f %.0f), lache (%.0f, %.0f) viseur (%.0f %.0f %.0f %.0f)\n",
								col != nullptr ? static_cast<int32>(col->forme) : -1, avant, NbEntites(m.scene), ui.placerOnglet, k,
								static_cast<double>(ui.placerRects[static_cast<uint32>(k)].x), static_cast<double>(ui.placerRects[static_cast<uint32>(k)].y),
								static_cast<double>(ui.placerRects[static_cast<uint32>(k)].w), static_cast<double>(ui.placerRects[static_cast<uint32>(k)].h),
								static_cast<double>(e.x), static_cast<double>(e.y), static_cast<double>(v.x), static_cast<double>(v.y),
								static_cast<double>(v.w), static_cast<double>(v.h));
				}
				Temoin(col != nullptr && col->forme == NkForme2D::NK_CERCLE && NbEntites(m.scene) == avant && m.selection == rect,
					   "(p8) volume (cercle) lache sur un rectangle : son collisionneur, sans entite de plus", 0.f);
			}

			// (p9) « Ajouter un composant > Collisionneur »
			{
				NkEditeurCadre c = T.Cadre();
				// La caisse TEXTUREE de la scene neuve : son contour.
				ecs::NkEntityId caisse;
				m.scene.Monde().Query<NkSprite2D>().ForEach([&](ecs::NkEntityId id, NkSprite2D &s) {
					if (s.texId != 0u && !caisse.IsValid()) {
						caisse = id;
					}
				});
				m.selection = caisse;
				m.aSelection = caisse.IsValid();
				NkEditeurExecuter(c, NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_DEPUIS_SPRITE));
				const NkCollisionneur2D *cs = m.scene.Monde().Get<NkCollisionneur2D>(caisse);
				const bool sprite = cs != nullptr && (cs->forme == NkForme2D::NK_BOITE || cs->forme == NkForme2D::NK_POLYGONE);
				NkEditeurExecuter(c, NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_POLYGONE));
				cs = m.scene.Monde().Get<NkCollisionneur2D>(caisse);
				const bool polygone = cs != nullptr && cs->forme == NkForme2D::NK_POLYGONE && cs->nbSommets == 5u &&
									  m.scene.Monde().Has<NkCorps2D>(caisse);
				const ecs::NkEntityId etoile = NkEditeurPoserForme(m, NkGenreForme2D::NK_ETOILE, NkVec2f(-6.f, 3.f));
				NkEditeurExecuter(c, NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_BOITE));
				NkEditeurExecuter(c, NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_DEPUIS_FORME));
				const NkCollisionneur2D *ce = m.scene.Monde().Get<NkCollisionneur2D>(etoile);
				Temoin(sprite && polygone && ce != nullptr && ce->forme == NkForme2D::NK_CHAINE && ce->boucle,
					   "(p9) collisionneur : contour du sprite, polygone, depuis la forme (chaine)", 0.f);
			}

			// (p10) les poignees dans la vue
			{
				const nkgui::NkRect v = ui.viseur;
				const NkVec2f lieu = m.scene.Camera().EcranVersMonde(NkVec2f(v.x + v.w * 0.62f, v.y + v.h * 0.55f));
				const ecs::NkEntityId e = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, lieu, true); // carre 1 x 1
				ui.editionCollision = true;
				T.Trame();
				// Le coin haut droit (0,5 ; 0,5) tire de 0,5 m vers la droite.
				const NkVec2f coin = Ecran(m.scene, NkVec2f(lieu.x + 0.5f, lieu.y + 0.5f));
				const NkVec2f vers = Ecran(m.scene, NkVec2f(lieu.x + 1.0f, lieu.y + 0.5f));
				ui.accrocheGrille = false;
				T.Aller(coin.x, coin.y);
				T.Glisser(coin.x, coin.y, vers.x, vers.y);
				const NkCollisionneur2D *col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const bool coinOk = col != nullptr && math::NkAbs(col->demiTaille.x - 1.0f) < 0.02f && math::NkAbs(col->demiTaille.y - 0.5f) < 0.02f;
				// La rotation : la poignee est au-dessus ; tiree vers la droite, -90 degres.
				const float32 px = 1.f / m.scene.Camera().Zoom();
				const NkVec2f rot = Ecran(m.scene, NkVec2f(lieu.x, lieu.y + 0.5f + 28.f * px));
				const NkVec2f droite = Ecran(m.scene, NkVec2f(lieu.x + 2.f, lieu.y));
				T.Aller(rot.x, rot.y);
				T.Glisser(rot.x, rot.y, droite.x, droite.y);
				col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const float32 angle = col != nullptr ? col->rotation * 57.2957795f : 0.f;
				const bool rotOk = math::NkAbs(angle + 90.f) < 3.f;
				// Un polygone : « + » au milieu du premier cote, puis Ctrl+clic sur un sommet.
				NkEditeurCadre c = T.Cadre();
				NkEditeurExecuter(c, NK_A_COLLISION + static_cast<int32>(NkCollisionEditeur::NK_POLYGONE));
				col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const uint32 n0 = col != nullptr ? col->nbSommets : 0u;
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(e);
				const NkVec2f mil = Ecran(m.scene, NkPointCollision2D(*t, *col, NkVec2f((col->sommets[0].x + col->sommets[1].x) * 0.5f,
																						 (col->sommets[0].y + col->sommets[1].y) * 0.5f)));
				T.Aller(mil.x, mil.y);
				T.Clic(mil.x, mil.y);
				col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const uint32 n1 = col->nbSommets;
				const NkVec2f s0 = Ecran(m.scene, NkPointCollision2D(*m.scene.Monde().Get<NkTransform2D>(e), *col, col->sommets[0]));
				T.Aller(s0.x, s0.y);
				T.Ctrl(true);
				T.Clic(s0.x, s0.y);
				T.Ctrl(false);
				col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const uint32 n2 = col->nbSommets;
				const bool corps = m.scene.Monde().Has<NkCorps2D>(e);
				ui.editionCollision = false;
				Temoin(coinOk && rotOk && n0 == 5u && n1 == 6u && n2 == 5u && corps,
					   "(p10) poignees : coin (demi 1,0), rotation (-90), + sommet, Ctrl+clic (deg)", angle);
			}

			// (p11) la trame avec une forme choisie, Details et poignees allumes
			{
				const ecs::NkEntityId e = NkEditeurPoserForme(m, NkGenreForme2D::NK_POLYGONE_LIBRE, NkVec2f(5.f, 3.f));
				ui.editionCollision = true;
				ui.ongletDroite = 0;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				ui.ongletDroite = 1; // l'onglet Monde : la section des calques
				T.Trame();
				ui.ongletDroite = 0;
				ui.editionCollision = false;
				Temoin(m.scene.Monde().IsAlive(e) && m.selection == e, "(p11) la trame dessine Details, Monde et poignees d'une forme", 0.f);
			}

			// (p12) Enregistrer / Ouvrir
			{
				const NkString dossier("unkeny_banc_formes");
				NkDirectory::Create(dossier.CStr());
				m.chemin = NkString::Format("%s/formes.nkscene", dossier.CStr());
				m.scene.Calques().Poser(3u, 4u, false);
				std::snprintf(m.scene.Calques().noms[3], sizeof(m.scene.Calques().noms[3]), "%s", "Ennemis");
				const uint32 formes = Compter(m.scene);
				const bool sauve = NkEditeurSauver(m);
				NkEditeurNouvelleScene(m);
				const bool vide = Compter(m.scene) == 0u;
				m.chemin = NkString::Format("%s/formes.nkscene", dossier.CStr());
				const bool ouvert = NkEditeurOuvrir(m);
				uint32 etoiles = 0;
				m.scene.Monde().Query<NkRenduForme2D>().ForEach([&](ecs::NkEntityId, NkRenduForme2D &f) {
					etoiles += f.genre == NkGenreForme2D::NK_ETOILE ? 1u : 0u;
				});
				const bool calques = !m.scene.Calques().Touche(3u, 4u) && std::strcmp(m.scene.Calques().noms[3], "Ennemis") == 0;
				Temoin(sauve && vide && ouvert && Compter(m.scene) == formes && etoiles >= 3u && calques,
					   "(p12) Enregistrer / Ouvrir : formes, genres et calques reviennent", static_cast<float32>(formes));
				NkDirectory::Delete(dossier.CStr(), true);
			}

			// (p13) Jouer : une etoile dynamique tombe sur un rectangle
			{
				NkEditeurNouvelleScene(m);
				const ecs::NkEntityId sol = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, NkVec2f(0.f, 0.f));
				NkRenduForme2D *fs = m.scene.Monde().Get<NkRenduForme2D>(sol);
				fs->taille = NkVec2f(4.f, 0.4f);
				NkEditeurFormeChangee(m, sol);
				const ecs::NkEntityId eto = NkEditeurPoserForme(m, NkGenreForme2D::NK_ETOILE, NkVec2f(0.f, 3.f));
				NkCorps2D *b = m.scene.Monde().Get<NkCorps2D>(eto);
				b->type = NkTypeCorps::NK_DYNAMIQUE;
				NkEditeurFormeChangee(m, eto); // dynamique : son enveloppe convexe
				m.scene.ActualiserCorps(eto);
				const uint64 uid = m.scene.Uid(eto);
				NkEditeurJouer(m);
				for (int32 k = 0; k < 150; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				const ecs::NkEntityId e2 = m.scene.EntiteParUid(uid);
				const float32 y = e2.IsValid() ? m.scene.Monde().Get<NkTransform2D>(e2)->position.y : 99.f;
				// Le dessus du sol a 0,2 m ; l'enveloppe de l'etoile (1,2 m) : apotheme 0,485.
				const bool pose = math::NkAbs(y - (0.2f + 0.4854f)) < 0.05f;
				NkEditeurArreter(m);
				const ecs::NkEntityId e3 = m.scene.EntiteParUid(uid);
				const float32 y3 = e3.IsValid() ? m.scene.Monde().Get<NkTransform2D>(e3)->position.y : 0.f;
				Temoin(pose && math::NkAbs(y3 - 3.f) < 1.0e-4f, "(p13) Jouer : l'etoile tombe et se pose ; Arreter la rend (y pose m)", y);
			}

			// ── L'AIMANT (Rihen, 01/10 ; NkEditeurAimant.cpp) ────────────────
			// Loin du decor de la scene neuve : A1 carre 1 x 1 en (20, 20), A2
			// rectangle 3 x 3 en (40, 20), B carre 1 x 1 deplace. 100 px/m : le
			// rayon de 12 px vaut 0,12 m.
			{
				NkEditeurNouvelleScene(m);
				NkVue2D &cam = m.scene.Camera();
				ui.cadrageAnime = false;
				ui.cadrageEnAttente = false;
				cam.PoserCentre(NkVec2f(20.8f, 20.8f));
				cam.PoserZoom(100.f);
				const ecs::NkEntityId a1 = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, NkVec2f(20.f, 20.f), true);
				const ecs::NkEntityId a2 = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, NkVec2f(40.f, 20.f), true);
				m.scene.Monde().Get<NkRenduForme2D>(a2)->taille = NkVec2f(3.f, 3.f);
				NkEditeurFormeChangee(m, a2);
				const ecs::NkEntityId b = NkEditeurPoserForme(m, NkGenreForme2D::NK_RECTANGLE, NkVec2f(21.6f, 21.6f), true);
				m.selection = b;
				m.aSelection = true;
				NkEditeurCadre c = T.Cadre();
				auto Pres = [](const NkVec2f &u, float32 x, float32 y) {
					return math::NkAbs(u.x - x) < 1.0e-3f && math::NkAbs(u.y - y) < 1.0e-3f;
				};
				// (a1) eteint par defaut : la cible passe telle quelle.
				ui.aimant = false;
				const NkVec2f libre = NkEditeurAimanterDeplacement(c, NkVec2f(21.06f, 21.05f));
				Temoin(Pres(libre, 21.06f, 21.05f) && !NkEditeurAimantActif(c), "(a1) aimant eteint par defaut : rien ne colle", 0.f);
				ui.aimant = true;
				// (a2) SOMMET : le coin bas-gauche de B a 8 cm du coin haut-droit de A1.
				const NkVec2f s = NkEditeurAimanterDeplacement(c, NkVec2f(21.06f, 21.05f));
				Temoin(Pres(s, 21.f, 21.f) && ui.aimantGenre == static_cast<int32>(NkGenreAimant::NK_SOMMET),
					   "(a2) sommet : coin contre coin, exactement (B en 21 ; 21)", s.x);
				// (a3) ARETE : loin des coins, a 5 cm du dessus de A1.
				const NkVec2f a = NkEditeurAimanterDeplacement(c, NkVec2f(20.3f, 21.05f));
				Temoin(Pres(a, 20.3f, 21.f) && ui.aimantGenre == static_cast<int32>(NkGenreAimant::NK_ARETE),
					   "(a3) arete : le bas de B pose sur le dessus de A1, x garde", a.y);
				// (a4) FACE : B enfonce de 0,7 m dans A2 (au-dela du rayon) : ramene a sa surface.
				const NkVec2f f = NkEditeurAimanterDeplacement(c, NkVec2f(40.f, 21.3f));
				Temoin(Pres(f, 40.f, 22.f) && ui.aimantGenre == static_cast<int32>(NkGenreAimant::NK_FACE),
					   "(a4) face : B enfonce dans A2 ressort sur sa surface (y 22)", f.y);
				// (a5) PRIORITE : un coin a 3 cm d'une arete et 8,5 cm d'un sommet : le sommet.
				const NkVec2f pr = NkEditeurAimanterDeplacement(c, NkVec2f(20.92f, 21.03f));
				Temoin(Pres(pr, 21.f, 21.f) && ui.aimantGenre == static_cast<int32>(NkGenreAimant::NK_SOMMET),
					   "(a5) priorite sommet > arete : le coin, pas l'arete plus proche", pr.x);
				// (a6) une POIGNEE (sommet d'un collisionneur) tiree pres du coin de A1.
				const NkVec2f pg = NkEditeurAimanterPoignee(c, NkVec2f(20.55f, 20.54f), b);
				Temoin(Pres(pg, 20.5f, 20.5f), "(a6) poignee tiree pres d'un coin : posee dessus", pg.x);
				// (a7) V maintenue l'allume le temps du geste ; (a8) le bouton de la barre.
				ui.aimant = false;
				T.pctx->input.keyDown[static_cast<int32>(nkgui::NkGuiKey::V)] = true;
				const NkVec2f v = NkEditeurAimanterDeplacement(c, NkVec2f(21.06f, 21.05f));
				T.pctx->input.keyDown[static_cast<int32>(nkgui::NkGuiKey::V)] = false;
				NkEditeurExecuter(c, NK_A_AIMANT);
				const bool bouton = ui.aimant;
				Temoin(Pres(v, 21.f, 21.f) && bouton, "(a7) V maintenue l'allume ; (a8) le bouton de la barre aussi", 0.f);
				// (a9) dans la VRAIE trame : B glisse a la souris vers le coin de A1.
				T.Trame();
				const NkVec2f depart = cam.MondeVersEcran(NkVec2f(21.6f, 21.6f));
				const NkVec2f arrivee = cam.MondeVersEcran(NkVec2f(21.06f, 21.05f));
				T.Aller(depart.x, depart.y);
				T.Appui(depart.x, depart.y);
				for (int32 k = 1; k <= 6; ++k) {
					const float32 t = static_cast<float32>(k) / 6.f;
					T.Aller(depart.x + (arrivee.x - depart.x) * t, depart.y + (arrivee.y - depart.y) * t);
				}
				const NkVec2f pendant = m.scene.Monde().Get<NkTransform2D>(b)->position;
				const bool vu = ui.aimantVu;
				T.Relache(arrivee.x, arrivee.y);
				const NkVec2f apres = m.scene.Monde().Get<NkTransform2D>(b)->position;
				Temoin(Pres(pendant, 21.f, 21.f) && vu && Pres(apres, 21.f, 21.f) && m.selection == b,
					   "(a9) vraie trame : B glisse et colle coin contre coin (x m)", apres.x);
				ui.aimant = false;
				(void)a1;
			}

			memory::NkGetDefaultAllocator().Delete(pt);
			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC FORMES EDITEUR REUSSI" : "BANC FORMES EDITEUR EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

		// =====================================================================
		// LES CAPTURES HORS ECRAN (--captures-formes=DOSSIER)
		// =====================================================================
		namespace {
			/// La trame courante, RASTERISEE sans fenetre ni GPU (NkGuiDrawListRaster,
			/// la sonde de NkAnimaEditor), puis ecrite en PNG (NKImage).
			bool EcrirePng(NkTrame &T, const char *chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				for (uint32 k = 0; k < T.m.textures.Nombre(); ++k) {
					const uint32 id = NkTextures2D::kPremierId + k;
					int32 tw = 0, th = 0;
					if (T.m.textures.Taille(id, tw, th) && T.m.textures.Pixels(id) != nullptr) {
						ras->PoserTexture(id, T.m.textures.Pixels(id), tw, th, 4);
					}
				}
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin);
				std::printf("  capture %s : %s\n", chemin, ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}
		} // namespace

		int32 NkEditeurCapturesFormes(const char *dossier) {
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurSceneFormes(m);
			NkTrame *pt = memory::NkGetDefaultAllocator().New<NkTrame>(m);
			NkTrame &T = *pt;
			T.W = 1600.f;
			T.H = 900.f;
			T.pctx->Init(1600, 900);
			NkEditeurInterface &ui = T.Ui();
			ui.hauteurTiroir = 150.f;
			ui.placerOnglet = static_cast<int32>(NkOngletPlacer::NK_FORMES);
			for (int32 k = 0; k < 4; ++k) {
				T.Trame();
			}
			// L'AIMANT : le Carre glisse vers le coin du Rectangle et s'y colle.
			ecs::NkEntityId carre;
			m.scene.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
				if (std::strcmp(e.nom, "Carré") == 0) {
					carre = id;
				}
			});
			NkVue2D &cam = m.scene.Camera();
			ui.cadrageAnime = false;
			cam.PoserCentre(NkVec2f(-8.4f, -1.8f));
			cam.PoserZoom(110.f);
			ui.aimant = true;
			int32 erreurs = 0;
			if (carre.IsValid()) {
				m.selection = carre;
				m.aSelection = true;
				T.Trame();
				const NkVec2f depart = cam.MondeVersEcran(m.scene.Monde().Get<NkTransform2D>(carre)->position);
				const NkVec2f arrivee = cam.MondeVersEcran(NkVec2f(-8.21f, -1.12f));
				T.Aller(depart.x, depart.y);
				T.Appui(depart.x, depart.y);
				for (int32 k = 1; k <= 8; ++k) {
					const float32 t = static_cast<float32>(k) / 8.f;
					T.Aller(depart.x + (arrivee.x - depart.x) * t, depart.y + (arrivee.y - depart.y) * t);
				}
				T.Aller(arrivee.x, arrivee.y); // l'indicateur se peint a la trame suivante
				erreurs += EcrirePng(T, NkString::Format("%s/06_aimant_sommet.png", dossier).CStr()) ? 0 : 1;
				T.Relache(arrivee.x, arrivee.y);
			} else {
				++erreurs;
			}
			// Les REGLAGES DU PROJET : les calques nommes de l'exemple et la matrice.
			ui.aimant = false;
			ui.reglagesCollision = true;
			T.Aller(-100.f, -100.f);
			T.Aller(-100.f, -100.f);
			erreurs += EcrirePng(T, NkString::Format("%s/05_reglages_calques.png", dossier).CStr()) ? 0 : 1;
			memory::NkGetDefaultAllocator().Delete(pt);
			memory::NkGetDefaultAllocator().Delete(pm);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
