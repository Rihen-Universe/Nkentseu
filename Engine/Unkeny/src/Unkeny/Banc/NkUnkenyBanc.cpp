// =============================================================================
// NkUnkenyBanc.cpp — textures, dessin texture, animation par images
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (t1)  une image chargee SANS rendu attend ; au branchement elle part, avec
//         ses dimensions et ses pixels exacts
//   (t2)  le meme nom rend le meme identifiant, deux noms deux identifiants,
//         et aucun identifiant ne tombe sous kPremierId (polices, logo)
//   (t3)  un BMP 24 bits de 3x2 (lignes REMPLIES a 12 octets) se charge en RGBA
//         jointif, pixel juste ; (t3n) un fichier absent rend 0
//   (t4)  un sprite texture est dessine avec SA texture, teintee par sa couleur
//         (blanc = l'image) ; (t4n) texId = 0 : l'aplat de couleur, comme avant
//   (t5)  ORIENTATION : les quatre coins d'une texture 2x2 arrivent aux quatre
//         coins du sprite a l'ecran — rouge en haut a gauche
//   (t6)  une region d'atlas (uv0/uv1) ne montre que sa case
//   (t7)  NkAnimSprite2D : une boucle de 4 images a 8 i/s est sur l'image 2
//         a 0,30 s et ecrit uv0.x = 0,5 dans le sprite ; UNE_FOIS se termine ;
//         l'evenement ne dure qu'UNE trame
//   (t8)  la photo de la scene garde l'animation (temps, clip)
//   (t9)  Reteleverser renvoie tout ; garderPixels = false libere les pixels
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}

			// Un faux rendu : il garde la derniere image recue.
			struct Recepteur {
					uint32 recues = 0;
					uint32 dernierId = 0;
					int32 w = 0, h = 0;
					uint8 premier[4] = {};
					NkVector<uint32> ids;
			};
			bool Recevoir(void *ctx, uint32 id, const uint8 *rgba, int32 w, int32 h) {
				Recepteur &r = *static_cast<Recepteur *>(ctx);
				++r.recues;
				r.dernierId = id;
				r.w = w;
				r.h = h;
				std::memcpy(r.premier, rgba, 4);
				r.ids.PushBack(id);
				return true;
			}

			/// Un BMP 24 bits ecrit A LA MAIN : il eprouve le decodeur sans passer
			/// par l'encodeur du depot (un aller-retour par le meme code pourrait
			/// avoir la meme erreur dans les deux sens).
			bool EcrireBmp(const char *chemin, int32 w, int32 h, const uint8 *rgb) {
				const int32 ligne = (w * 3 + 3) & ~3; // lignes remplies a 4 octets
				const uint32 taille = 54u + static_cast<uint32>(ligne * h);
				uint8 en[54] = {'B', 'M'};
				auto u32 = [&](int32 at, uint32 v) {
					for (int32 k = 0; k < 4; ++k) {
						en[at + k] = static_cast<uint8>(v >> (8 * k));
					}
				};
				u32(2, taille);
				u32(10, 54u);
				u32(14, 40u);
				u32(18, static_cast<uint32>(w));
				u32(22, static_cast<uint32>(h));
				en[26] = 1;
				en[28] = 24;
				u32(34, static_cast<uint32>(ligne * h));
				FILE *f = std::fopen(chemin, "wb");
				if (f == nullptr) {
					return false;
				}
				std::fwrite(en, 1, 54, f);
				for (int32 y = h - 1; y >= 0; --y) { // BMP : la ligne du BAS d'abord
					uint8 buf[64] = {};
					for (int32 x = 0; x < w; ++x) {
						const uint8 *p = rgb + (y * w + x) * 3;
						buf[x * 3 + 0] = p[2]; // B G R
						buf[x * 3 + 1] = p[1];
						buf[x * 3 + 2] = p[0];
					}
					std::fwrite(buf, 1, static_cast<usize>(ligne), f);
				}
				std::fclose(f);
				return true;
			}

			/// Dessine la scene et rend la cible rasterisee. La camera montre
			/// [-2, 2] x [-2, 2] dans 200 x 200 pixels : 50 px par unite.
			struct Image {
					nkgui::NkGuiContext ctx;
					nkgui::NkGuiDrawListRaster raster;
					void Rendre(NkScene &s, const NkTextures2D &tex) {
						s.Camera().PoserViseur(NkRect{0.f, 0.f, 200.f, 200.f});
						s.Camera().PoserCentre(NkVec2f(0.f, 0.f));
						s.Camera().PoserZoom(50.f);
						ctx.Init(200, 200);
						ctx.BeginFrame(1.f / 60.f);
						NkDessinerScene(ctx.dl, s);
						ctx.EndFrame();
						raster.Init(200, 200);
						raster.Effacer(0x000000FFu);
						for (uint32 id = NkTextures2D::kPremierId; id < NkTextures2D::kPremierId + tex.Nombre(); ++id) {
							int32 w = 0, h = 0;
							if (tex.Taille(id, w, h) && tex.Pixels(id) != nullptr) {
								raster.PoserTexture(id, tex.Pixels(id), w, h, 4);
							}
						}
						raster.Rasteriser(ctx.dl);
					}
					uint32 Px(int32 x, int32 y) const {
						const uint8 *p = raster.Pixels() + (y * raster.Largeur() + x) * 4;
						return (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) | (static_cast<uint32>(p[2]) << 8) | 0xFFu;
					}
			};

			bool Proche(uint32 a, uint32 b, int32 tol = 6) {
				for (int32 k = 8; k < 32; k += 8) {
					const int32 x = static_cast<int32>((a >> k) & 0xFFu), y = static_cast<int32>((b >> k) & 0xFFu);
					if (x - y > tol || y - x > tol) {
						return false;
					}
				}
				return true;
			}

			ecs::NkEntityId Sprite(NkScene &s, const NkSprite2D &sp) {
				const ecs::NkEntityId e = s.Creer(NkVec2f(0.f, 0.f));
				s.Monde().Add<NkSprite2D>(e, sp);
				return e;
			}
		} // namespace

		int32 NkUnkenyLancerBanc() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("Unkeny — banc du moteur (textures, dessin, animation)\n\n");

			// Une texture 2x2 : rouge | vert / bleu | blanc.
			const uint8 quatre[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};

			// (t1) (t2)
			{
				NkTextures2D tex;
				const uint32 a = tex.Creer(quatre, 2, 2, "quatre");
				Temoin(tex.EnAttente() == 1u, "(t1) chargee sans rendu : elle attend", static_cast<float32>(tex.EnAttente()));
				Recepteur r;
				tex.Brancher(&Recevoir, &r);
				Temoin(tex.EnAttente() == 0u && r.recues == 1u && r.dernierId == a && r.w == 2 && r.h == 2 && r.premier[0] == 255 &&
						   r.premier[1] == 0,
					   "(t1) au branchement : partie, 2x2, premier pixel rouge", static_cast<float32>(r.recues));
				const uint32 b = tex.Creer(quatre, 2, 2, "autre");
				const uint32 a2 = tex.Trouver("quatre");
				Temoin(a2 == a && b != a && a >= NkTextures2D::kPremierId && b >= NkTextures2D::kPremierId,
					   "(t2) meme nom meme id, deux noms deux ids, hors plage reservee", static_cast<float32>(b - a));
			}

			// (t3) BMP a lignes remplies
			{
				const uint8 rgb[18] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180};
				const char *chemin = "unkeny_banc_3x2.bmp";
				const bool ecrit = EcrireBmp(chemin, 3, 2, rgb);
				NkTextures2D tex;
				tex.PoserRacine("");
				const uint32 id = ecrit ? tex.Charger(chemin) : 0u;
				int32 w = 0, h = 0;
				const uint8 *p = tex.Pixels(id);
				bool juste = id != 0u && tex.Taille(id, w, h) && w == 3 && h == 2 && p != nullptr;
				for (int32 i = 0; juste && i < 6; ++i) {
					juste = p[i * 4] == rgb[i * 3] && p[i * 4 + 1] == rgb[i * 3 + 1] && p[i * 4 + 2] == rgb[i * 3 + 2] && p[i * 4 + 3] == 255;
				}
				Temoin(juste, "(t3) BMP 3x2 (lignes de 12 octets) -> RGBA jointif, 6 pixels justes", static_cast<float32>(id != 0u));
				Temoin(tex.Charger(chemin) == id, "(t3) recharger le meme fichier ne le relit pas (meme id)", 1.f);
				std::remove(chemin);
				Temoin(tex.Charger("n_existe_pas.png") == 0u, "(t3n) fichier absent : 0", 0.f);
			}

			// (t4) (t5) (t6) dessin texture
			{
				NkScene s;
				NkSceneConfig cfg;
				s.Init(cfg);
				NkTextures2D tex;
				const uint32 id = tex.Creer(quatre, 2, 2, "quatre");
				NkSprite2D sp = tex.Sprite(id, 1.f); // 2 px d'image = 2 m
				sp.taille = NkVec2f(2.f, 2.f);
				const ecs::NkEntityId e = Sprite(s, sp); // pivot 0,5 : de -1 a 1 -> pixels 50..150
				Image im;
				im.Rendre(s, tex);
				// Le centre de chaque quart : (75, 75) haut-gauche, (125, 75) haut-droit...
				const uint32 hg = im.Px(75, 75), hd = im.Px(125, 75), bg = im.Px(75, 125), bd = im.Px(125, 125);
				Temoin(Proche(hg, 0xFF0000FFu) && Proche(hd, 0x00FF00FFu) && Proche(bg, 0x0000FFFFu) && Proche(bd, 0xFFFFFFFFu),
					   "(t5) coins de la texture = coins du sprite (rouge en haut a gauche)", static_cast<float32>(hg >> 24));
				Temoin(Proche(im.Px(20, 20), 0x000000FFu), "(t5) hors du sprite : le fond, rien d'autre", 0.f);

				s.Monde().Get<NkSprite2D>(e)->couleur = 0x808080FFu;
				im.Rendre(s, tex);
				Temoin(Proche(im.Px(125, 125), 0x808080FFu, 8), "(t4) la couleur TEINTE la texture (blanc x gris = gris)",
					   static_cast<float32>(im.Px(125, 125) >> 24));
				s.Monde().Get<NkSprite2D>(e)->couleur = 0xFFFFFFFFu;

				s.Monde().Get<NkSprite2D>(e)->texId = 0u;
				s.Monde().Get<NkSprite2D>(e)->couleur = 0x336699FFu;
				im.Rendre(s, tex);
				Temoin(Proche(im.Px(75, 75), 0x336699FFu) && Proche(im.Px(125, 125), 0x336699FFu), "(t4n) texId = 0 : l'aplat de couleur",
					   0.f);

				// Un vrai atlas : 2 x 2 cases de 8 x 8 pixels. (Avec des cases d'UN
				// pixel, le filtre bilineaire melange forcement les voisines : c'est
				// le « saignement » d'atlas, et il ne dit rien du calcul des UV.)
				uint8 atlas[16 * 16 * 4];
				for (int32 y = 0; y < 16; ++y) {
					for (int32 x = 0; x < 16; ++x) {
						const uint8 *src = quatre + ((y / 8) * 2 + (x / 8)) * 4;
						std::memcpy(atlas + (y * 16 + x) * 4, src, 4);
					}
				}
				const uint32 idAtlas = tex.Creer(atlas, 16, 16, "atlas");
				NkSprite2D *q = s.Monde().Get<NkSprite2D>(e);
				q->texId = idAtlas;
				q->couleur = 0xFFFFFFFFu;
				NkTextures2D::RegionGrille(2, 2, 1, q->uv0, q->uv1); // la case haut-droite : verte
				im.Rendre(s, tex);
				Temoin(Proche(im.Px(60, 60), 0x00FF00FFu) && Proche(im.Px(140, 140), 0x00FF00FFu), "(t6) une case d'atlas ne montre qu'elle",
					   q->uv0.x);
			}

			// (t7) (t8) animation
			{
				NkScene s;
				NkSceneConfig cfg;
				s.Init(cfg);
				NkSprite2D sp;
				const ecs::NkEntityId e = Sprite(s, sp);
				NkAnimSprite2D a;
				a.colonnes = 4;
				a.lignes = 2;
				a.nbClips = 2;
				a.clips[0].premiere = 0;
				a.clips[0].nombre = 4;
				a.clips[0].imagesParSeconde = 8.f;
				a.clips[0].imageEvent = 3;
				a.clips[1].premiere = 4;
				a.clips[1].nombre = 4;
				a.clips[1].imagesParSeconde = 8.f;
				a.clips[1].mode = NkModeLecture::NK_UNE_FOIS;
				s.Monde().Add<NkAnimSprite2D>(e, a);
				int32 evenements = 0;
				for (int32 k = 0; k < 18; ++k) { // 0,30 s
					s.Pas(1.f / 60.f);
					evenements += s.Monde().Get<NkAnimSprite2D>(e)->evenementAtteint ? 1 : 0;
				}
				const NkAnimSprite2D *an = s.Monde().Get<NkAnimSprite2D>(e);
				const NkSprite2D *su = s.Monde().Get<NkSprite2D>(e);
				Temoin(an->ImageCourante() == 2 && su->uv0.x > 0.49f && su->uv0.x < 0.51f && su->uv1.y > 0.49f && su->uv1.y < 0.51f,
					   "(t7) boucle 4 images a 8 i/s : image 2 a 0,30 s, uv0.x = 0,5", su->uv0.x);
				for (int32 k = 0; k < 12; ++k) { // jusqu'a 0,50 s : l'image 3 est passee
					s.Pas(1.f / 60.f);
					evenements += s.Monde().Get<NkAnimSprite2D>(e)->evenementAtteint ? 1 : 0;
				}
				Temoin(evenements == 1, "(t7) l'evenement de l'image 3 dure UNE trame", static_cast<float32>(evenements));

				s.Monde().Get<NkAnimSprite2D>(e)->Jouer(1);
				for (int32 k = 0; k < 40; ++k) {
					s.Pas(1.f / 60.f);
				}
				an = s.Monde().Get<NkAnimSprite2D>(e);
				Temoin(an->termine && an->ImageCourante() == 7 && su->uv0.y > 0.49f, "(t7) UNE_FOIS : terminee sur sa derniere image (7)",
					   static_cast<float32>(an->ImageCourante()));

				// (t8)
				NkScene::NkPhoto photo;
				s.Photographier(photo);
				s.Monde().Get<NkAnimSprite2D>(e)->Jouer(0);
				s.Restaurer(photo);
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				const NkAnimSprite2D *r = ids.Size() == 1u ? s.Monde().Get<NkAnimSprite2D>(ids[0]) : nullptr;
				Temoin(r != nullptr && r->clipCourant == 1 && r->termine && r->nbClips == 2,
					   "(t8) la photo garde l'animation (clip, fin, clips)", r != nullptr ? static_cast<float32>(r->clipCourant) : -1.f);
			}

			// (t9)
			{
				NkTextures2D tex;
				Recepteur r;
				tex.Brancher(&Recevoir, &r);
				tex.Creer(quatre, 2, 2, "a");
				tex.Creer(quatre, 2, 2, "b");
				const uint32 avant = r.recues;
				const uint32 n = tex.Reteleverser();
				Temoin(n == 2u && r.recues == avant + 2u, "(t9) Reteleverser renvoie les 2 images", static_cast<float32>(n));
				NkTextures2D leger;
				leger.garderPixels = false;
				leger.Brancher(&Recevoir, &r);
				const uint32 id = leger.Creer(quatre, 2, 2, "c");
				Temoin(leger.Pixels(id) == nullptr && leger.EnAttente() == 0u, "(t9) garderPixels = false : pixels liberes apres envoi", 0.f);
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC UNKENY REUSSI" : "BANC UNKENY EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
