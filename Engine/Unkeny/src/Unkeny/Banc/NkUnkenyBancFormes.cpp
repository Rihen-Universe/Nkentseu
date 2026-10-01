//
// NkUnkenyBancFormes.cpp
// =============================================================================
// Description :
//   Le banc des FORMES 2D (NkUnkenyFormes.h), de leurs collisionneurs (polygone,
//   chaine, rotation) et des CALQUES DE COLLISION. Compte A PART (« BANC
//   FORMES ») : les bancs d'avant gardent leurs comptes.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (f1)  chaque genre SE DESSINE : un point dedans a la couleur du remplissage,
//         un point dehors (mais dans la boite de la forme) reste du fond --
//         etoile et polygone libre compris (leur CREUX est vide : triangules,
//         pas en eventail) ; (f1n) contre-epreuve : l'eventail d'un convexe
//         remplit le creux de l'etoile
//   (f2)  le contour : sur le bord, sa couleur ; au milieu, le remplissage ;
//         (f2n) sans epaisseur, pas de contour
//   (f3)  l'opacite 0,5 sur du noir donne un rouge a moitie (128 +/- 20)
//   (f4)  l'arrondi vide le coin d'un carre ; sans arrondi, il est plein
//   (f5)  le tri par COUCHE est partage avec les sprites (devant, puis derriere)
//   (f6)  le collisionneur assorti, genre par genre, decor et dynamique
//   (f7)  triangle, hexagone, etoile DYNAMIQUES tombent sur un sol rectangle et
//         s'arretent a la hauteur de leur sommet le plus bas
//   (f8)  une balle tombe DANS une coupe concave (chaine exacte du decor) ;
//         (f8n) avec l'enveloppe convexe, elle reste posee sur le dessus
//   (f9)  enregistrer / relire / enregistrer : le meme JSON a l'octet, formes,
//         collisionneurs (polygone, chaine, rotation) et calques compris ;
//         (f9b) une scene SANS rien de neuf n'ecrit aucune cle neuve
//   (f10) un prefab d'une forme : l'instance porte la forme, et son fichier aussi
//   (f11) calques : une caisse du calque 1 traverse un sol du calque 2 quand la
//         matrice le dit, s'y pose sinon ; AppliquerCalques refait les corps
//   (f12) une boite STATIQUE inclinee fait rouler la balle (avant : plate pour
//         les rigides) ; (f12n) a plat, elle ne roule pas
//   (f13) l'obstacle oblique (capsule tournee) fait rouler la balle (avant :
//         la capsule restait couchee pour le solveur)
//   (f14) le contour d'un sprite : un disque opaque donne un polygone de 8
//         sommets au plus, dans le sprite, d'aire voisine du disque ; une image
//         sans transparence donne sa boite
//   (f15) la prise au clic d'une forme : dedans, dans le creux, sur une ligne
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyFormes.h"
#include "Unkeny/Scene/NkUnkenyPrefab.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Simulation/NkUnkenyActeurs.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			/// Une scene SUR LE TAS (la pile des bancs est deja lourde).
			struct NkSceneTas {
					NkScene *s = nullptr;
					explicit NkSceneTas(bool physique) {
						s = memory::NkGetDefaultAllocator().New<NkScene>();
						NkSceneConfig cfg;
						cfg.physique = physique;
						s->Init(cfg);
					}
					~NkSceneTas() {
						memory::NkGetDefaultAllocator().Delete(s);
					}
					NkScene &operator*() noexcept {
						return *s;
					}
			};

			/// L'image : [-2, 2] x [-2, 2] dans 200 x 200 pixels, 50 px/m. Le monde
			/// (x, y) tombe au pixel (100 + 50 x, 100 - 50 y). Fond NOIR.
			struct NkImage {
					nkgui::NkGuiContext ctx;
					nkgui::NkGuiDrawListRaster raster;
					void Rendre(NkScene &s) {
						s.Camera().PoserViseur(nkgui::NkRect{0.f, 0.f, 200.f, 200.f});
						s.Camera().PoserCentre(NkVec2f(0.f, 0.f));
						s.Camera().PoserZoom(50.f);
						ctx.Init(200, 200);
						ctx.BeginFrame(1.f / 60.f);
						NkDessinerScene(ctx.dl, s);
						ctx.EndFrame();
						raster.Init(200, 200);
						raster.Effacer(0x000000FFu);
						raster.Rasteriser(ctx.dl);
					}
					/// Le pixel sous le point du MONDE (x, y), en 0xRRGGBBFF.
					uint32 Px(float32 x, float32 y) const {
						int32 px = static_cast<int32>(100.f + 50.f * x);
						int32 py = static_cast<int32>(100.f - 50.f * y);
						px = px < 0 ? 0 : (px > 199 ? 199 : px);
						py = py < 0 ? 0 : (py > 199 ? 199 : py);
						const uint8 *p = raster.Pixels() + (py * raster.Largeur() + px) * 4;
						return (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) |
							   (static_cast<uint32>(p[2]) << 8) | 0xFFu;
					}
			};

			struct NkImageTas {
					NkImage *i = nullptr;
					NkImageTas() {
						i = memory::NkGetDefaultAllocator().New<NkImage>();
					}
					~NkImageTas() {
						memory::NkGetDefaultAllocator().Delete(i);
					}
					NkImage *operator->() noexcept {
						return i;
					}
			};

			int32 Canal(uint32 px, int32 k) {
				return static_cast<int32>((px >> (24 - 8 * k)) & 0xFFu);
			}

			bool Proche(uint32 a, uint32 b, int32 tol = 24) {
				for (int32 k = 0; k < 3; ++k) {
					const int32 d = Canal(a, k) - Canal(b, k);
					if (d > tol || -d > tol) {
						return false;
					}
				}
				return true;
			}

			constexpr uint32 ROUGE = 0xFF0000FFu;
			constexpr uint32 VERT = 0x00FF00FFu;
			constexpr uint32 BLEU = 0x0000FFFFu;
			constexpr uint32 NOIR = 0x000000FFu;

			NkRenduForme2D Forme(NkGenreForme2D g, float32 w, float32 h) {
				NkRenduForme2D f = NkFormeParDefaut(g);
				f.taille = NkVec2f(w, h);
				f.remplissage = ROUGE;
				f.epaisseurContour = 0.f;
				return f;
			}

			/// Dessine `f` seule, au centre, et rend vrai si chaque point de
			/// `dedans` est rouge et chaque point de `dehors` noir.
			bool Dessinee(const NkRenduForme2D &f, const NkVec2f *dedans, int32 nd, const NkVec2f *dehors, int32 nh) {
				NkSceneTas s(false);
				const ecs::NkEntityId e = (*s).Creer("F", NkVec2f(0.f, 0.f));
				(*s).Monde().Add<NkRenduForme2D>(e, f);
				NkImageTas im;
				im->Rendre(*s);
				bool ok = true;
				for (int32 i = 0; i < nd; ++i) {
					ok = ok && Proche(im->Px(dedans[i].x, dedans[i].y), ROUGE);
				}
				for (int32 i = 0; i < nh; ++i) {
					ok = ok && Proche(im->Px(dehors[i].x, dehors[i].y), NOIR);
				}
				return ok;
			}

			/// Pas fixes de la scene pendant `secondes`.
			void Simuler(NkScene &s, float32 secondes) {
				const int32 n = static_cast<int32>(secondes * 60.f + 0.5f);
				for (int32 i = 0; i < n; ++i) {
					s.Pas(1.f / 60.f);
				}
			}

			NkVec2f Position(NkScene &s, ecs::NkEntityId e) {
				const NkTransform2D *t = s.Monde().Get<NkTransform2D>(e);
				return t != nullptr ? t->position : NkVec2f(0.f, 0.f);
			}

			/// Le sol : un rectangle STATIQUE de 8 x 0,4 m dont le dessus est a y = 0.
			ecs::NkEntityId Sol(NkScene &s, uint32 couche = 0x1u) {
				NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_RECTANGLE);
				f.taille = NkVec2f(8.f, 0.4f);
				const ecs::NkEntityId e = NkPoserForme2D(s, f, NkVec2f(0.f, -0.2f), "Sol", true, -1);
				s.Monde().Get<NkCollisionneur2D>(e)->couche = couche;
				NkCorps2D c;
				c.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(e, c);
				return e;
			}

			ecs::NkEntityId Balle(NkScene &s, const NkVec2f &pos, float32 r) {
				NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_CERCLE);
				f.taille = NkVec2f(2.f * r, 2.f * r);
				return NkPoserForme2D(s, f, pos, "Balle", true, static_cast<int32>(NkTypeCorps::NK_DYNAMIQUE));
			}

			bool Contient(const NkString &s, const char *t) {
				return std::strstr(s.CStr(), t) != nullptr;
			}

			// =================================================================
			void BancDessin() {
				// (f1) chaque genre
				{
					const NkVec2f in[2] = {NkVec2f(0.8f, 0.4f), NkVec2f(-0.8f, -0.4f)};
					const NkVec2f out[1] = {NkVec2f(1.2f, 0.f)};
					Temoin(Dessinee(Forme(NkGenreForme2D::NK_RECTANGLE, 2.f, 1.f), in, 2, out, 1),
						   "(f1) rectangle : dedans rempli, dehors vide", 0.f);
				}
				{
					const NkVec2f in[1] = {NkVec2f(0.6f, 0.6f)};
					const NkVec2f out[1] = {NkVec2f(0.8f, 0.8f)}; // dans sa boite, hors du disque
					Temoin(Dessinee(Forme(NkGenreForme2D::NK_CERCLE, 2.f, 2.f), in, 1, out, 1),
						   "(f1) cercle : dedans rempli, le coin de sa boite vide", 0.f);
				}
				{
					const NkVec2f in[1] = {NkVec2f(1.3f, 0.f)};
					const NkVec2f out[1] = {NkVec2f(1.0f, 0.5f)};
					Temoin(Dessinee(Forme(NkGenreForme2D::NK_ELLIPSE, 3.f, 1.2f), in, 1, out, 1),
						   "(f1) ellipse : le long du grand axe rempli, hors d'elle vide", 0.f);
				}
				{
					const NkVec2f in[1] = {NkVec2f(0.f, 0.f)};
					const NkVec2f out[1] = {NkVec2f(0.8f, 0.6f)};
					Temoin(Dessinee(Forme(NkGenreForme2D::NK_TRIANGLE, 2.f, 1.5f), in, 1, out, 1),
						   "(f1) triangle : centroide rempli, hors du flanc vide", 0.f);
				}
				// L'etoile : son CREUX (entre deux branches) reste vide.
				const NkVec2f creux(-0.588f, 0.809f); // 126 degres, a 1 m (sommet rentrant a 0,6 m)
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_ETOILE, 3.f, 3.f);
					f.branches = 5;
					f.rayonInterieur = 0.4f;
					const NkVec2f in[2] = {NkVec2f(0.f, 0.f), NkVec2f(0.f, 1.35f)};
					const NkVec2f out[1] = {creux};
					Temoin(Dessinee(f, in, 2, out, 1), "(f1) etoile : centre et branche remplis, CREUX vide", 0.f);
					// (f1n) La contre-epreuve : le meme contour en EVENTAIL (le dessin
					// d'un convexe) remplit le creux.
					NkVec2f pts[NK_FORME_CONTOUR_MAX];
					bool ferme = true;
					const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
					NkSceneTas s(false);
					NkImageTas im;
					im->Rendre(*s); // camera posee, image vide
					for (uint32 i = 0; i < n; ++i) {
						pts[i] = (*s).Camera().MondeVersEcran(pts[i]);
					}
					im->ctx.BeginFrame(1.f / 60.f);
					im->ctx.dl.AddConvexPolyFilled(pts, static_cast<int32>(n), nkgui::NkColor(255, 0, 0, 255));
					im->ctx.EndFrame();
					im->raster.Effacer(0x000000FFu);
					im->raster.Rasteriser(im->ctx.dl);
					Temoin(Proche(im->Px(creux.x, creux.y), ROUGE), "(f1n) contre-epreuve : l'eventail remplit le creux", 0.f);
				}
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_POLYGONE_REGULIER, 2.f, 2.f);
					f.cotes = 6;
					const NkVec2f in[2] = {NkVec2f(0.f, 0.8f), NkVec2f(0.95f, 0.f)};
					const NkVec2f out[1] = {NkVec2f(0.f, 0.95f)};
					Temoin(Dessinee(f, in, 2, out, 1), "(f1) hexagone : sous le cote du haut rempli, au-dessus vide", 0.f);
				}
				{
					const NkVec2f in[1] = {NkVec2f(1.3f, 0.f)};
					const NkVec2f out[1] = {NkVec2f(1.45f, 0.45f)};
					Temoin(Dessinee(Forme(NkGenreForme2D::NK_CAPSULE, 3.f, 1.f), in, 1, out, 1),
						   "(f1) capsule : bout rond rempli, coin de sa boite vide", 0.f);
				}
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_LIGNE, 3.f, 2.f);
					f.nbPoints = 2;
					f.points[0] = NkVec2f(-1.5f, -1.f);
					f.points[1] = NkVec2f(1.5f, 1.f);
					f.epaisseur = 0.2f;
					const NkVec2f in[2] = {NkVec2f(0.f, 0.f), NkVec2f(1.2f, 0.8f)};
					const NkVec2f out[1] = {NkVec2f(0.f, 0.3f)};
					Temoin(Dessinee(f, in, 2, out, 1), "(f1) ligne : le trait rempli, a cote vide", 0.f);
				}
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_POLYGONE_LIBRE, 3.2f, 2.f);
					for (uint32 i = 0; i < f.nbPoints; ++i) {
						f.points[i] = NkVec2f(f.points[i].x * 2.f, f.points[i].y * 2.f);
					}
					const NkVec2f in[2] = {NkVec2f(-1.f, 0.f), NkVec2f(0.8f, 0.3f)};
					const NkVec2f out[1] = {NkVec2f(0.f, 0.7f)};
					Temoin(Dessinee(f, in, 2, out, 1), "(f1) polygone libre (fleche) : hampe et pointe, creux vide", 0.f);
				}

				// (f2) le contour
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_RECTANGLE, 2.f, 1.f);
					f.epaisseurContour = 0.12f;
					f.couleurContour = VERT;
					NkSceneTas s(false);
					const ecs::NkEntityId e = (*s).Creer("F", NkVec2f(0.f, 0.f));
					(*s).Monde().Add<NkRenduForme2D>(e, f);
					NkImageTas im;
					im->Rendre(*s);
					const bool bord = Proche(im->Px(1.0f, 0.f), VERT) && Proche(im->Px(0.f, 0.5f), VERT);
					const bool milieu = Proche(im->Px(0.5f, 0.f), ROUGE);
					Temoin(bord && milieu, "(f2) contour : vert sur le bord, rouge au milieu", 0.f);
					(*s).Monde().Get<NkRenduForme2D>(e)->epaisseurContour = 0.f;
					im->Rendre(*s);
					Temoin(!Proche(im->Px(1.0f, 0.f), VERT), "(f2n) sans epaisseur : pas de contour", 0.f);
				}
				// (f3) l'opacite
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_RECTANGLE, 2.f, 1.f);
					f.opacite = 0.5f;
					NkSceneTas s(false);
					(*s).Monde().Add<NkRenduForme2D>((*s).Creer("F", NkVec2f(0.f, 0.f)), f);
					NkImageTas im;
					im->Rendre(*s);
					const int32 r = Canal(im->Px(0.f, 0.f), 0);
					Temoin(r > 108 && r < 148, "(f3) opacite 0,5 sur du noir : rouge a moitie (canal R)", static_cast<float32>(r));
				}
				// (f4) l'arrondi
				{
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_RECTANGLE, 2.f, 2.f);
					f.arrondi = 0.5f;
					const NkVec2f in[1] = {NkVec2f(0.5f, 0.5f)};
					const NkVec2f coin[1] = {NkVec2f(0.93f, 0.93f)};
					const bool arrondi = Dessinee(f, in, 1, coin, 1);
					f.arrondi = 0.f;
					const bool vif = Dessinee(f, coin, 1, nullptr, 0);
					Temoin(arrondi && vif, "(f4) arrondi 0,5 : le coin est vide ; sans arrondi, plein", 0.f);
				}
				// (f5) les couches, partagees avec les sprites
				{
					NkSceneTas s(false);
					NkSprite2D sp;
					sp.taille = NkVec2f(1.f, 1.f);
					sp.couleur = BLEU;
					sp.couche = 0;
					(*s).Monde().Add<NkSprite2D>((*s).Creer("Sprite", NkVec2f(0.f, 0.f)), sp);
					NkRenduForme2D f = Forme(NkGenreForme2D::NK_RECTANGLE, 1.f, 1.f);
					f.couche = 1;
					const ecs::NkEntityId e = (*s).Creer("Forme", NkVec2f(0.f, 0.f));
					(*s).Monde().Add<NkRenduForme2D>(e, f);
					NkImageTas im;
					im->Rendre(*s);
					const bool devant = Proche(im->Px(0.f, 0.f), ROUGE);
					(*s).Monde().Get<NkRenduForme2D>(e)->couche = -1;
					im->Rendre(*s);
					const bool derriere = Proche(im->Px(0.f, 0.f), BLEU);
					Temoin(devant && derriere, "(f5) couche : la forme devant (1), puis derriere (-1) le sprite", 0.f);
				}
			}

			// =================================================================
			void BancCollisionneurs() {
				// (f6) le collisionneur assorti
				struct NkAttendu {
						NkGenreForme2D g;
						bool dynamique;
						NkForme2D forme;
						int32 sommets; ///< -1 : non verifie
				};
				const NkAttendu kAttendus[] = {
					{NkGenreForme2D::NK_RECTANGLE, false, NkForme2D::NK_BOITE, -1},
					{NkGenreForme2D::NK_CERCLE, true, NkForme2D::NK_CERCLE, -1},
					{NkGenreForme2D::NK_ELLIPSE, true, NkForme2D::NK_POLYGONE, 8},
					{NkGenreForme2D::NK_ELLIPSE, false, NkForme2D::NK_CHAINE, 24},
					{NkGenreForme2D::NK_TRIANGLE, true, NkForme2D::NK_POLYGONE, 3},
					{NkGenreForme2D::NK_ETOILE, false, NkForme2D::NK_CHAINE, 10},
					{NkGenreForme2D::NK_ETOILE, true, NkForme2D::NK_POLYGONE, 5},
					{NkGenreForme2D::NK_POLYGONE_REGULIER, true, NkForme2D::NK_POLYGONE, 6},
					{NkGenreForme2D::NK_CAPSULE, true, NkForme2D::NK_CAPSULE, -1},
					{NkGenreForme2D::NK_LIGNE, false, NkForme2D::NK_CHAINE, 8},
					{NkGenreForme2D::NK_POLYGONE_LIBRE, false, NkForme2D::NK_CHAINE, 7},
					{NkGenreForme2D::NK_POLYGONE_LIBRE, true, NkForme2D::NK_POLYGONE, 5},
				};
				int32 bons = 0;
				const int32 n = static_cast<int32>(sizeof(kAttendus) / sizeof(kAttendus[0]));
				for (int32 i = 0; i < n; ++i) {
					NkCollisionneur2D c;
					const bool ok = NkCollisionneurDepuisForme(NkFormeParDefaut(kAttendus[i].g), kAttendus[i].dynamique, c);
					const bool bonNombre = kAttendus[i].sommets < 0 || static_cast<int32>(c.nbSommets) == kAttendus[i].sommets;
					if (ok && c.forme == kAttendus[i].forme && bonNombre) {
						++bons;
					} else {
						std::printf("      (f6) %s (%s) : forme %d, %u sommets\n", NkNomGenreForme2D(kAttendus[i].g),
									kAttendus[i].dynamique ? "dynamique" : "decor", static_cast<int32>(c.forme),
									static_cast<unsigned>(c.nbSommets));
					}
				}
				// Un polygone regulier de 12 cotes, dynamique et inscrit dans un carre : un cercle.
				NkRenduForme2D douze = NkFormeParDefaut(NkGenreForme2D::NK_POLYGONE_REGULIER);
				douze.cotes = 12;
				NkCollisionneur2D c12;
				NkCollisionneurDepuisForme(douze, true, c12);
				bons += c12.forme == NkForme2D::NK_CERCLE ? 1 : 0;
				Temoin(bons == n + 1, "(f6) collisionneur assorti : genre par genre (bons sur 13)", static_cast<float32>(bons));

				// (f7) chutes
				{
					NkSceneTas s(true);
					Sol(*s);
					NkRenduForme2D tri = NkFormeParDefaut(NkGenreForme2D::NK_TRIANGLE);
					tri.taille = NkVec2f(1.f, 1.f);
					NkRenduForme2D hex = NkFormeParDefaut(NkGenreForme2D::NK_POLYGONE_REGULIER);
					hex.taille = NkVec2f(1.f, 1.f);
					NkRenduForme2D eto = NkFormeParDefaut(NkGenreForme2D::NK_ETOILE);
					eto.taille = NkVec2f(1.2f, 1.2f);
					const int32 dyn = static_cast<int32>(NkTypeCorps::NK_DYNAMIQUE);
					const ecs::NkEntityId a = NkPoserForme2D(*s, tri, NkVec2f(-2.f, 2.f), "Triangle", true, dyn);
					const ecs::NkEntityId b = NkPoserForme2D(*s, hex, NkVec2f(0.f, 2.f), "Hexagone", true, dyn);
					const ecs::NkEntityId c = NkPoserForme2D(*s, eto, NkVec2f(2.f, 2.f), "Etoile", true, dyn);
					Simuler(*s, 3.f);
					const float32 ya = Position(*s, a).y, yb = Position(*s, b).y, yc = Position(*s, c).y;
					const NkVec2f va = (*s).Vitesse(a), vb = (*s).Vitesse(b), vc = (*s).Vitesse(c);
					const float32 vmax = math::NkMax(math::NkMax(math::NkAbs(va.y), math::NkAbs(vb.y)), math::NkAbs(vc.y));
					// Le plus bas de chacun : 1/3 (centroide du triangle), 0,433 (hexagone
					// a plat), 0,485 (deux pointes de l'etoile, son enveloppe).
					const float32 ecart = math::NkMax(math::NkMax(math::NkAbs(ya - 0.3333f), math::NkAbs(yb - 0.4330f)),
													  math::NkAbs(yc - 0.4854f));
					Temoin(ecart < 0.04f && vmax < 0.05f, "(f7) triangle, hexagone, etoile tombent et se posent (ecart max m)", ecart);
				}
				// (f8) la coupe concave, chaine exacte du decor
				for (int32 enveloppe = 0; enveloppe < 2; ++enveloppe) {
					NkSceneTas s(true);
					NkRenduForme2D coupe = NkFormeParDefaut(NkGenreForme2D::NK_POLYGONE_LIBRE);
					const NkVec2f pts[8] = {NkVec2f(-1.5f, 1.5f), NkVec2f(-1.5f, 0.f), NkVec2f(1.5f, 0.f), NkVec2f(1.5f, 1.5f),
											NkVec2f(1.2f, 1.5f),  NkVec2f(1.2f, 0.3f), NkVec2f(-1.2f, 0.3f), NkVec2f(-1.2f, 1.5f)};
					coupe.nbPoints = 8;
					for (int32 i = 0; i < 8; ++i) {
						coupe.points[i] = pts[i];
					}
					const ecs::NkEntityId e = NkPoserForme2D(*s, coupe, NkVec2f(0.f, 0.f), "Coupe", true, -1);
					if (enveloppe == 1) {
						// La contre-epreuve : l'enveloppe convexe (le collisionneur d'un
						// corps dynamique) sur ce meme decor.
						NkCollisionneurDepuisForme(coupe, true, *(*s).Monde().Get<NkCollisionneur2D>(e));
					}
					NkCorps2D c;
					c.type = NkTypeCorps::NK_STATIQUE;
					(*s).AjouterCorps(e, c);
					const ecs::NkEntityId balle = Balle(*s, NkVec2f(0.f, 2.5f), 0.2f);
					Simuler(*s, 2.5f);
					const float32 y = Position(*s, balle).y;
					if (enveloppe == 0) {
						Temoin(math::NkAbs(y - 0.5f) < 0.05f, "(f8) balle DANS la coupe concave (y m, attendu 0,5)", y);
					} else {
						Temoin(y > 1.6f, "(f8n) contre-epreuve, enveloppe convexe : posee dessus (y m)", y);
					}
				}
			}

			// =================================================================
			void BancSauvegarde() {
				NkSceneTas s(true);
				for (int32 g = 0; g < static_cast<int32>(NkGenreForme2D::NK_COUNT); ++g) {
					NkRenduForme2D f = NkFormeParDefaut(static_cast<NkGenreForme2D>(g));
					f.epaisseurContour = 0.05f * static_cast<float32>(g);
					f.arrondi = g == 0 ? 0.2f : 0.f;
					f.opacite = 0.9f;
					f.couche = g;
					f.branches = 7;
					f.cotes = 5;
					NkPoserForme2D(*s, f, NkVec2f(static_cast<float32>(g) - 4.f, 1.f), nullptr, true,
								   g % 2 == 0 ? static_cast<int32>(NkTypeCorps::NK_STATIQUE) : static_cast<int32>(NkTypeCorps::NK_DYNAMIQUE));
				}
				// Un collisionneur tourne, a la main.
				const ecs::NkEntityId t = (*s).Creer("Tourne", NkVec2f(0.f, -1.f));
				NkCollisionneur2D col;
				col.rotation = 0.3f;
				col.demiTaille = NkVec2f(1.f, 0.2f);
				(*s).Monde().Add<NkCollisionneur2D>(t, col);
				(*s).Calques().Poser(1u, 2u, false);
				std::snprintf((*s).Calques().noms[1], sizeof((*s).Calques().noms[1]), "%s", "Joueur");
				std::snprintf((*s).Calques().noms[2], sizeof((*s).Calques().noms[2]), "%s", "Balles du joueur");
				NkString a, b;
				const bool sa = NkSauverSceneJSON(*s, a);
				NkSceneTas r(true);
				NkString err;
				const bool lu = NkChargerSceneJSON(*r, a.View(), static_cast<NkTextures2D *>(nullptr), &err);
				const bool sb = NkSauverSceneJSON(*r, b);
				uint32 formes = 0;
				bool etoile = false;
				(*r).Monde().Query<NkRenduForme2D>().ForEach([&](ecs::NkEntityId, NkRenduForme2D &f) {
					++formes;
					etoile = etoile || (f.genre == NkGenreForme2D::NK_ETOILE && f.branches == 7);
				});
				const bool calques = !(*r).Calques().Touche(1u, 2u) && !(*r).Calques().Touche(2u, 1u) &&
									 std::strcmp((*r).Calques().noms[2], "Balles du joueur") == 0;
				Temoin(sa && lu && sb && a == b && formes == static_cast<uint32>(NkGenreForme2D::NK_COUNT) && etoile && calques,
					   "(f9) enregistrer / relire / enregistrer : meme JSON, formes et calques", static_cast<float32>(a.Length()));
				if (!(a == b)) {
					std::printf("      (f9) relu : %s\n", err.CStr());
				}
				// (f9b) une scene d'avant : aucune cle neuve.
				NkSceneTas v(true);
				Sol(*v);
				NkString c;
				NkSauverSceneJSON(*v, c);
				// Le sol d'ici porte une forme : on la retire, il reste ce qu'on ecrivait avant.
				NkSceneTas w(true);
				const ecs::NkEntityId sol = (*w).Creer("Sol", NkVec2f(0.f, 0.f));
				(*w).Monde().Add<NkCollisionneur2D>(sol, NkCollisionneur2D());
				NkCorps2D cs;
				cs.type = NkTypeCorps::NK_STATIQUE;
				(*w).AjouterCorps(sol, cs);
				NkString d;
				NkSauverSceneJSON(*w, d);
				Temoin(Contient(c, "NkRenduForme2D") && !Contient(d, "NkRenduForme2D") && !Contient(d, "calques") &&
						   !Contient(d, "\"rotation\"") && !Contient(d, "sommets") && !Contient(d, "boucle"),
					   "(f9b) une scene sans rien de neuf : aucune cle neuve ecrite", 0.f);

				// (f10) le prefab d'une forme
				{
					NkPrefabs2D prefabs;
					NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_ETOILE);
					f.branches = 6;
					f.remplissage = 0x123456FFu;
					const ecs::NkEntityId e = NkPoserForme2D(*s, f, NkVec2f(5.f, 5.f), "EtoileModele", true, -1);
					const uint32 p = prefabs.Creer(*s, e, "etoile");
					const ecs::NkEntityId i = p != 0u ? prefabs.Instancier(*s, p, NkVec2f(-5.f, 5.f)) : ecs::NkEntityId();
					const NkRenduForme2D *fi = i.IsValid() ? (*s).Monde().Get<NkRenduForme2D>(i) : nullptr;
					NkString json;
					NkRessourcesScene rs;
					const bool ecrit = p != 0u && prefabs.EnregistrerJSON(*s, p, json, rs);
					Temoin(fi != nullptr && fi->branches == 6 && fi->remplissage == 0x123456FFu && ecrit &&
							   Contient(json, "NkRenduForme2D") && (*s).Monde().Has<NkCollisionneur2D>(i),
						   "(f10) prefab d'une etoile : l'instance et le fichier portent la forme", 0.f);
				}
			}

			// =================================================================
			void BancCalquesEtRotations() {
				// (f11) les calques
				float32 yTraverse = 0.f, yPose = 0.f, yApres = 0.f;
				for (int32 cas = 0; cas < 3; ++cas) {
					NkSceneTas s(true);
					if (cas == 0) {
						(*s).Calques().Poser(1u, 2u, false);
					}
					Sol(*s, 1u << 2u);
					NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_RECTANGLE);
					f.taille = NkVec2f(0.6f, 0.6f);
					const ecs::NkEntityId e = NkPoserForme2D(*s, f, NkVec2f(0.f, 1.f), "Caisse", true, -1);
					(*s).Monde().Get<NkCollisionneur2D>(e)->couche = 1u << 1u;
					NkCorps2D c;
					(*s).AjouterCorps(e, c);
					Simuler(*s, 1.5f);
					if (cas == 2) {
						// Posee, puis la matrice change : AppliquerCalques refait les corps.
						(*s).Calques().Poser(1u, 2u, false);
						(*s).AppliquerCalques();
						Simuler(*s, 1.5f);
					}
					(cas == 0 ? yTraverse : (cas == 1 ? yPose : yApres)) = Position(*s, e).y;
				}
				Temoin(yTraverse < -1.f && math::NkAbs(yPose - 0.3f) < 0.03f && yApres < -1.f,
					   "(f11) calques 1 / 2 : traverse, se pose, AppliquerCalques (y pose m)", yPose);

				// (f12) une boite STATIQUE inclinee
				for (int32 incline = 1; incline >= 0; --incline) {
					NkSceneTas s(true);
					NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_RECTANGLE);
					f.taille = NkVec2f(4.f, 0.4f);
					const ecs::NkEntityId e = NkPoserForme2D(*s, f, NkVec2f(0.f, 0.f), "Pente", true, -1);
					(*s).Monde().Get<NkTransform2D>(e)->rotation = incline != 0 ? 0.5236f : 0.f;
					NkCorps2D c;
					c.type = NkTypeCorps::NK_STATIQUE;
					(*s).AjouterCorps(e, c);
					const ecs::NkEntityId balle = Balle(*s, NkVec2f(0.5f, 2.f), 0.15f);
					Simuler(*s, 1.5f);
					const float32 x = Position(*s, balle).x;
					if (incline != 0) {
						Temoin(x < 0.f, "(f12) boite statique inclinee de 30 degres : la balle roule (x m)", x);
					} else {
						Temoin(math::NkAbs(x - 0.5f) < 0.05f, "(f12n) a plat : elle ne roule pas (x m)", x);
					}
				}
				// (f13) l'obstacle oblique (capsule tournee)
				{
					NkSceneTas s(true);
					NkPoserObstacleSim(*s, NkVec2f(-2.f, 0.f), NkVec2f(2.f, 2.f), 0.1f);
					const ecs::NkEntityId balle = Balle(*s, NkVec2f(1.f, 3.f), 0.15f);
					Simuler(*s, 1.5f);
					const float32 x = Position(*s, balle).x;
					Temoin(x < 0.5f, "(f13) obstacle oblique (capsule tournee) : la balle roule (x m)", x);
				}
			}

			// =================================================================
			void BancSpriteEtPrise() {
				// (f14) le contour d'un sprite
				const int32 W = 32;
				NkVector<uint8> disque;
				NkVector<uint8> plein;
				disque.Resize(static_cast<usize>(W * W * 4));
				plein.Resize(static_cast<usize>(W * W * 4));
				for (int32 y = 0; y < W; ++y) {
					for (int32 x = 0; x < W; ++x) {
						const float32 dx = static_cast<float32>(x) + 0.5f - 16.f, dy = static_cast<float32>(y) + 0.5f - 16.f;
						const usize i = static_cast<usize>((y * W + x) * 4);
						disque[i] = disque[i + 1] = disque[i + 2] = 255;
						disque[i + 3] = (dx * dx + dy * dy <= 14.f * 14.f) ? 255 : 0;
						plein[i] = plein[i + 1] = plein[i + 2] = plein[i + 3] = 255;
					}
				}
				NkSprite2D sp;
				sp.taille = NkVec2f(1.f, 1.f);
				NkCollisionneur2D c;
				const bool ok = NkCollisionneurDepuisSprite(sp, disque.Data(), W, W, c);
				float32 aire = 0.f;
				bool dedans = true;
				if (ok && c.forme == NkForme2D::NK_POLYGONE) {
					aire = NkAireSignee2D(c.sommets, c.nbSommets);
					for (uint32 i = 0; i < c.nbSommets; ++i) {
						dedans = dedans && math::NkAbs(c.sommets[i].x) <= 0.5f + 1.0e-4f && math::NkAbs(c.sommets[i].y) <= 0.5f + 1.0e-4f;
					}
				}
				NkCollisionneur2D b;
				const bool okb = NkCollisionneurDepuisSprite(sp, plein.Data(), W, W, b);
				Temoin(ok && c.forme == NkForme2D::NK_POLYGONE && c.nbSommets <= 8u && dedans && aire > 0.45f && aire < 0.66f && okb &&
						   b.forme == NkForme2D::NK_BOITE && math::NkAbs(b.demiTaille.x - 0.5f) < 1.0e-4f,
					   "(f14) contour d'un sprite : disque -> polygone <= 8, opaque -> boite (aire)", aire);

				// (f15) la prise au clic
				NkTransform2D t;
				NkRenduForme2D eto = NkFormeParDefaut(NkGenreForme2D::NK_ETOILE);
				eto.taille = NkVec2f(3.f, 3.f);
				eto.rayonInterieur = 0.4f;
				NkRenduForme2D lig = NkFormeParDefaut(NkGenreForme2D::NK_LIGNE);
				const float32 d0 = NkDistanceRenduForme2D(t, eto, NkVec2f(0.f, 0.f));
				const float32 d1 = NkDistanceRenduForme2D(t, eto, NkVec2f(-0.588f, 0.809f));
				const float32 d2 = NkDistanceRenduForme2D(t, lig, lig.points[1]);
				Temoin(d0 < 0.f && d1 > 0.f && d2 < 0.f, "(f15) prise : dedans, dans le creux (dehors), sur la ligne", d1);
			}
		} // namespace

		int32 NkUnkenyLancerBancFormes() {
			gE = 0;
			gR = 0;
			std::printf("\nUnkeny — banc des formes 2D, des collisionneurs et des calques\n\n");
			BancDessin();
			BancCollisionneurs();
			BancSauvegarde();
			BancCalquesEtRotations();
			BancSpriteEtPrise();
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC FORMES REUSSI" : "BANC FORMES EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
