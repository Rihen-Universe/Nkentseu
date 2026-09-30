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
//   (s1)  une scene (sol, caisse en vol, blob, sprite texture et anime, un
//         composant du jeu nomme) ecrite en JSON puis relue dans une AUTRE
//         scene rend : les memes entites, l'etat de la caisse, chaque particule
//         et chaque lien, la texture retrouvee PAR SON NOM, l'animation, le
//         composant du jeu ; (s1n) un composant jamais declare n'y est pas
//   (s2)  les deux scenes, avancees d'1 s, restent ensemble (centre du blob a
//         moins d'1 cm, caisse a moins d'1 mm) : c'est la MEME simulation
//   (s3)  un fichier qui n'est pas une scene, une version future, un tableau
//         de particules tronque : refuses, et la scene cible est INTACTE
//   (s4)  aller-retour par un vrai fichier sur disque
//   (a1)  spatialisation : au centre pan 0 / gain 1 ; au bord droit pan 0,8 ;
//         a mi-portee hors champ gain 0,25 ; au-dela de la portee, 0
//   (a2)  un son fabrique : meme nom meme id, duree juste
//   (a3)  SANS moteur audio : Jouer rend 0, Avancer calcule pan et gain, rien
//         ne casse
//   (a4)  avec un moteur a sortie NULLE : une boucle auDemarrage joue ; son
//         entite detruite, la voix est COUPEE a la trame suivante
//   (a5)  `demande` joue une fois, `arret` coupe
//   (a6)  apres Photographier / Restaurer, la boucle d'ambiance revit
//   (j1)  les systemes tournent par ordre croissant, puis par ordre d'ajout
//   (j2)  a 30 i/s pour un pas fixe de 1/60 : NK_PAS_FIXE deux fois (dt 1/60),
//         NK_TRAME une fois (dt 1/30) — et sans monde physique aussi
//   (j3)  un systeme desactive ne tourne pas ; un systeme qui se RETIRE
//         pendant le parcours ne casse rien et ne saute pas le suivant
//   (j4)  une balle lachee sur le sol : UN contact DEBUT balle/sol, en entites ;
//         (j4n) la meme balle lachee dans le vide : aucun
//   (j5)  une zone declencheur traversee : DEBUT puis FIN, `a` = la zone
//   (n1..n10) l'animateur (la HFSM de NKAnima) : NkUnkenyBancAnimateur.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKFileSystem/NkFile.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Son/NkUnkenySon.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		// Dans son propre fichier : il compte avec ce banc, par le meme Temoin.
		void NkUnkenyBancAnimateur(void (*temoin)(bool, const char *, float32));

		namespace {
			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}

			/// Attend (au plus 1 s) que la voix se taise : les coupures sont des
			/// fondus, que le mixeur termine sur son propre fil.
			bool AttendreSilence(const NkSons2D &sons, uint32 voix) {
				for (int32 k = 0; k < 100; ++k) {
					if (!sons.Joue(voix)) {
						return true;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				return !sons.Joue(voix);
			}

			// Deux composants "du jeu" pour la sauvegarde.
			struct NkBancMarque {
					uint32 code = 0;
					float32 valeur = 0.f;
			};
			struct NkBancSecret {
					uint32 x = 0;
			};

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

			// (s1) .. (s4) sauvegarde
			{
				auto Remplir = [&](NkScene &s, NkTextures2D &tex) {
					NkSceneConfig cfg;
					cfg.physique = true;
					cfg.particules = true;
					s.Init(cfg);
					s.PhotographierAussi<NkBancMarque>("banc.NkBancMarque");
					s.PhotographierAussi<NkBancSecret>(); // SANS nom : memoire seulement
					const ecs::NkEntityId sol = s.Creer("Sol", NkVec2f(0.f, -0.5f));
					NkCollisionneur2D cs;
					cs.demiTaille = NkVec2f(20.f, 0.5f);
					s.Monde().Add<NkCollisionneur2D>(sol, cs);
					NkCorps2D ks;
					ks.type = NkTypeCorps::NK_STATIQUE;
					s.AjouterCorps(sol, ks);
					const ecs::NkEntityId caisse = s.Creer("Caisse", NkVec2f(-2.f, 3.f));
					NkCollisionneur2D cc;
					cc.demiTaille = NkVec2f(0.3f, 0.3f);
					s.Monde().Add<NkCollisionneur2D>(caisse, cc);
					NkCorps2D kc;
					kc.masse = 12.f;
					kc.friction = 0.7f;
					s.AjouterCorps(caisse, kc);
					s.PoserVitesse(caisse, NkVec2f(1.5f, 2.f));
					NkSprite2D sp = tex.Sprite(tex.Trouver("quatre"), 1.f);
					s.Monde().Add<NkSprite2D>(caisse, sp);
					NkAnimSprite2D an;
					an.nbClips = 1;
					an.clips[0].nombre = 4;
					an.colonnes = 4;
					an.temps = 0.3f;
					s.Monde().Add<NkAnimSprite2D>(caisse, an);
					s.Monde().Add<NkBancMarque>(caisse, NkBancMarque{42u, 2.5f});
					s.Monde().Add<NkBancSecret>(caisse, NkBancSecret{7u});
					const int32 ci = physics::NkCreerBlobP2D(*s.Particules(), NkVec2f(1.f, 1.2f), 0.4f, physics::NkPresetP2D::NK_BLOB);
					s.CreerCorpsMou("Blob", ci, 0x7CE64CFFu);
					s.Particules()->reglages.limites.actif = false;
					for (int32 k = 0; k < 20; ++k) {
						s.Pas(1.f / 60.f); // un etat EN MOUVEMENT, pas celui de la creation
					}
				};
				auto Caisse = [](NkScene &s) {
					ecs::NkEntityId trouve = ecs::NkEntityId::Invalid();
					s.Monde().Query<NkCorps2D>().ForEach([&](ecs::NkEntityId id, NkCorps2D &k) {
						if (k.type == NkTypeCorps::NK_DYNAMIQUE) {
							trouve = id;
						}
					});
					return trouve;
				};
				auto Blob = [](NkScene &s) {
					const physics::NkParticules2D *p = s.Particules();
					return p != nullptr && p->corps.Size() > 0 ? p->CentreCorps(0) : NkVec2f(-99.f, -99.f);
				};

				NkTextures2D tex;
				tex.Creer(quatre, 2, 2, "quatre");
				NkScene a;
				Remplir(a, tex);
				NkString json;
				const bool ecrit = NkSauverSceneJSON(a, json, &tex);

				NkScene b;
				b.PhotographierAussi<NkBancMarque>("banc.NkBancMarque");
				b.PhotographierAussi<NkBancSecret>();
				NkString err;
				const bool lu = ecrit && NkChargerSceneJSON(b, json.View(), &tex, &err);
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}
				NkVector<ecs::NkEntityId> ea, eb;
				a.Entites(ea);
				b.Entites(eb);
				const physics::NkParticules2D &pa = *a.Particules();
				const physics::NkParticules2D *pb = b.Particules();
				float32 ecart = 0.f;
				bool memes = pb != nullptr && pb->particules.Size() == pa.particules.Size() && pb->liens.Size() == pa.liens.Size() &&
							 pb->corps.Size() == pa.corps.Size();
				for (uint32 i = 0; memes && i < pa.particules.Size(); ++i) {
					const NkVec2f d = pa.particules[i].pos - pb->particules[i].pos;
					const NkVec2f dv = pa.particules[i].vit - pb->particules[i].vit;
					ecart = math::NkMax(ecart, math::NkMax(math::NkAbs(d.x) + math::NkAbs(d.y), math::NkAbs(dv.x) + math::NkAbs(dv.y)));
					memes = memes && pa.particules[i].nbVoisins == pb->particules[i].nbVoisins;
				}
				for (uint32 i = 0; memes && i < pa.liens.Size(); ++i) {
					memes = pa.liens[i].a == pb->liens[i].a && pa.liens[i].b == pb->liens[i].b && pa.liens[i].repos == pb->liens[i].repos;
				}
				Temoin(lu && ea.Size() == eb.Size() && memes && ecart < 1.0e-6f,
					   "(s1) memes entites, chaque particule et chaque lien (ecart max m, m/s)", ecart);
				const ecs::NkEntityId ca = Caisse(a), cb = Caisse(b);
				const physics::NkRigidBody *ra = a.MondePhysique()->GetBody(a.Monde().Get<NkCorps2D>(ca)->corpsId);
				const physics::NkRigidBody *rb = lu && cb.IsValid() ? b.MondePhysique()->GetBody(b.Monde().Get<NkCorps2D>(cb)->corpsId) : nullptr;
				Temoin(rb != nullptr && ra->position.x == rb->position.x && ra->position.y == rb->position.y &&
						   ra->linearVelocity.y == rb->linearVelocity.y && rb->invMass > 0.08f && rb->invMass < 0.0834f,
					   "(s1) caisse : position, vitesse et masse (12 kg) a l'identique", rb != nullptr ? rb->linearVelocity.y : -1.f);
				const NkSprite2D *sb = lu && cb.IsValid() ? b.Monde().Get<NkSprite2D>(cb) : nullptr;
				const NkAnimSprite2D *ab = lu && cb.IsValid() ? b.Monde().Get<NkAnimSprite2D>(cb) : nullptr;
				Temoin(sb != nullptr && sb->texId == tex.Trouver("quatre") && ab != nullptr && ab->clips[0].nombre == 4,
					   "(s1) texture retrouvee par son nom, animation relue", sb != nullptr ? static_cast<float32>(sb->texId - NkTextures2D::kPremierId) : -1.f);
				const NkBancMarque *mb = lu && cb.IsValid() ? b.Monde().Get<NkBancMarque>(cb) : nullptr;
				Temoin(mb != nullptr && mb->code == 42u && mb->valeur == 2.5f, "(s1) le composant du jeu NOMME est relu", mb != nullptr ? mb->valeur : -1.f);
				Temoin(lu && cb.IsValid() && !b.Monde().Has<NkBancSecret>(cb), "(s1n) le composant SANS nom n'est pas dans le fichier", 0.f);

				// (s2) la meme simulation
				for (int32 k = 0; k < 60; ++k) {
					a.Pas(1.f / 60.f);
					b.Pas(1.f / 60.f);
				}
				const NkVec2f da = Blob(a) - Blob(b);
				const float32 dBlob = math::NkSqrt(da.x * da.x + da.y * da.y);
				const NkTransform2D *ta = a.Monde().Get<NkTransform2D>(ca);
				const NkTransform2D *tb = lu && cb.IsValid() ? b.Monde().Get<NkTransform2D>(cb) : nullptr;
				const float32 dCaisse = tb != nullptr ? math::NkAbs(ta->position.x - tb->position.x) + math::NkAbs(ta->position.y - tb->position.y) : 99.f;
				Temoin(dBlob < 0.01f && dCaisse < 0.001f, "(s2) apres 1 s : meme simulation (ecart du centre du blob m)", dBlob);

				// (s3) refus, scene intacte
				NkScene c;
				Remplir(c, tex);
				NkVector<ecs::NkEntityId> avant;
				c.Entites(avant);
				const uint32 n0 = static_cast<uint32>(c.Particules()->particules.Size());
				NkString faux = json;
				bool refus1 = !NkChargerSceneJSON(c, "{\"format\": \"autre.chose\"}", &tex, &err);
				bool refus2 = !NkChargerSceneJSON(c, "{\"format\": \"unkeny.scene\", \"version\": 999}", &tex, &err);
				// Tronquer le tableau des positions : il manque des nombres.
				const char *cle = "\"pos\": \"";
				const char *debut = std::strstr(faux.CStr(), cle);
				bool refus3 = false;
				if (debut != nullptr) {
					const usize at = static_cast<usize>(debut - faux.CStr()) + std::strlen(cle);
					NkString tronque(faux.CStr(), at);
					tronque.Append("1 2 3\"");
					const char *fin = std::strchr(faux.CStr() + at, '"');
					tronque.Append(fin + 1);
					refus3 = !NkChargerSceneJSON(c, tronque.View(), &tex, &err);
				}
				NkVector<ecs::NkEntityId> apres;
				c.Entites(apres);
				Temoin(refus1 && refus2 && refus3 && apres.Size() == avant.Size() && c.Particules()->particules.Size() == n0,
					   "(s3) format, version, tableau tronque : refuses, scene intacte", static_cast<float32>(refus1 + refus2 + refus3));

				// (s4) fichier
				const char *chemin = "unkeny_banc_scene.nkscene";
				NkScene d;
				const bool fichier = NkSauverSceneFichier(a, chemin, &tex) && NkChargerSceneFichier(d, chemin, &tex, &err);
				NkVector<ecs::NkEntityId> ed;
				d.Entites(ed);
				const NkString contenu = NkFile::ReadAllText(chemin);
				std::remove(chemin);
				Temoin(fichier && ed.Size() == ea.Size() && d.Particules()->particules.Size() == pa.particules.Size(),
					   "(s4) aller-retour par un fichier (taille en ko)", static_cast<float32>(contenu.Length()) / 1024.f);
			}

			// (a1) .. (a6) son
			{
				NkVue2D cam;
				cam.PoserViseur(NkRect{0.f, 0.f, 200.f, 100.f});
				cam.PoserCentre(NkVec2f(0.f, 0.f));
				cam.PoserZoom(10.f); // champ : x [-10, 10], y [-5, 5]
				float32 p0, g0, p1, g1, p2, g2, p3, g3;
				NkSons2D::Spatialiser(NkVec2f(0.f, 0.f), cam, 8.f, p0, g0);
				NkSons2D::Spatialiser(NkVec2f(10.f, 0.f), cam, 8.f, p1, g1);
				NkSons2D::Spatialiser(NkVec2f(14.f, 0.f), cam, 8.f, p2, g2);
				NkSons2D::Spatialiser(NkVec2f(-30.f, 0.f), cam, 8.f, p3, g3);
				Temoin(p0 == 0.f && g0 == 1.f && p1 > 0.79f && p1 < 0.81f && g1 == 1.f && g2 > 0.24f && g2 < 0.26f && p3 < -0.79f && g3 == 0.f,
					   "(a1) centre, bord, mi-portee, hors portee (gain a mi-portee)", g2);

				NkVector<float32> bip;
				bip.Resize(24000);
				for (uint32 i = 0; i < bip.Size(); ++i) {
					bip[i] = 0.2f * math::NkSin(static_cast<float32>(i) * 0.0572f);
				}
				{
					NkSons2D sons;
					const uint32 a = sons.Creer(bip.Data(), bip.Size(), 48000, "bip");
					Temoin(a != 0u && sons.Creer(bip.Data(), bip.Size(), 48000, "bip") == a && sons.Duree(a) > 0.499f && sons.Duree(a) < 0.501f,
						   "(a2) son fabrique : meme nom meme id, 0,5 s", sons.Duree(a));

					// (a3) sans moteur
					NkScene s;
					NkSceneConfig cfg;
					s.Init(cfg);
					s.Camera().PoserViseur(NkRect{0.f, 0.f, 200.f, 100.f});
					s.Camera().PoserZoom(10.f);
					const ecs::NkEntityId e = s.Creer(NkVec2f(10.f, 0.f));
					NkSource2D src;
					src.son = a;
					src.auDemarrage = true;
					src.boucle = true;
					s.Monde().Add<NkSource2D>(e, src);
					const uint32 v = sons.Jouer(a);
					sons.Avancer(s);
					const NkSource2D *r = s.Monde().Get<NkSource2D>(e);
					Temoin(!sons.Actif() && v == 0u && r->voix == 0u && r->pan > 0.79f && r->gain == 1.f,
						   "(a3) sans moteur : rien ne joue, pan et gain calcules", r->pan);
				}

				NkSons2D sons;
				const bool moteur = sons.Demarrer(true);
				const uint32 a = sons.Creer(bip.Data(), bip.Size(), 48000, "bip");
				NkScene s;
				NkSceneConfig cfg;
				s.Init(cfg);
				s.Camera().PoserViseur(NkRect{0.f, 0.f, 200.f, 100.f});
				s.Camera().PoserZoom(10.f);
				const ecs::NkEntityId e = s.Creer(NkVec2f(-4.f, 0.f));
				NkSource2D src;
				src.son = a;
				src.auDemarrage = true;
				src.boucle = true;
				s.Monde().Add<NkSource2D>(e, src);
				sons.Avancer(s);
				const uint32 v1 = s.Monde().Get<NkSource2D>(e)->voix;
				const bool joue1 = sons.Joue(v1);
				s.Detruire(e);
				sons.Avancer(s);
				// La coupure est un FONDU de 50 ms (sans lui, un claquement) : on
				// laisse au mixeur le temps de le finir.
				const bool coupee1 = AttendreSilence(sons, v1);
				Temoin(moteur && v1 != 0u && joue1 && coupee1, "(a4) boucle lancee, puis coupee a la destruction de l'entite",
					   static_cast<float32>(v1));

				const ecs::NkEntityId f = s.Creer(NkVec2f(0.f, 0.f));
				NkSource2D coup;
				coup.son = a;
				s.Monde().Add<NkSource2D>(f, coup);
				sons.Avancer(s);
				const bool rienAvant = s.Monde().Get<NkSource2D>(f)->voix == 0u;
				s.Monde().Get<NkSource2D>(f)->demande = true;
				sons.Avancer(s);
				const uint32 v2 = s.Monde().Get<NkSource2D>(f)->voix;
				const bool joue2 = sons.Joue(v2);
				s.Monde().Get<NkSource2D>(f)->arret = true;
				sons.Avancer(s);
				Temoin(rienAvant && v2 != 0u && joue2 && AttendreSilence(sons, v2) && s.Monde().Get<NkSource2D>(f)->voix == 0u,
					   "(a5) demande joue une fois, arret coupe", static_cast<float32>(v2));

				const ecs::NkEntityId g = s.Creer(NkVec2f(0.f, 0.f));
				NkSource2D amb = src;
				s.Monde().Add<NkSource2D>(g, amb);
				sons.Avancer(s);
				NkScene::NkPhoto photo;
				s.Photographier(photo);
				s.Restaurer(photo);
				sons.Avancer(s);
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				uint32 vivantes = 0;
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const NkSource2D *q = s.Monde().Get<NkSource2D>(ids[i]);
					if (q != nullptr && q->boucle && sons.Joue(q->voix)) {
						++vivantes;
					}
				}
				Temoin(vivantes == 1u, "(a6) apres Restaurer, la boucle d'ambiance revit (voix actives)", static_cast<float32>(vivantes));
				sons.Arreter();
			}

			// (j1) .. (j5) logique de jeu
			{
				struct Journal {
						char trace[64] = {};
						int32 n = 0;
						float32 dtFixe = 0.f, dtTrame = 0.f;
						int32 fixes = 0, trames = 0;
						uint32 aRetirer = 0;
				};
				static Journal j;
				j = Journal{};
				auto A = [](NkScene &, float32, void *) { j.trace[j.n++] = 'a'; };
				auto B = [](NkScene &, float32, void *) { j.trace[j.n++] = 'b'; };
				auto C = [](NkScene &, float32, void *) { j.trace[j.n++] = 'c'; };
				NkScene s;
				NkSceneConfig cfg; // SANS physique
				s.Init(cfg);
				s.AjouterSysteme("a", NkPhaseSysteme::NK_TRAME, A, nullptr, 5);
				s.AjouterSysteme("b", NkPhaseSysteme::NK_TRAME, B, nullptr, 0);
				s.AjouterSysteme("c", NkPhaseSysteme::NK_TRAME, C, nullptr, 0);
				s.Pas(1.f / 60.f);
				Temoin(std::strcmp(j.trace, "bca") == 0, "(j1) ordre croissant puis ordre d'ajout (b, c, a)", static_cast<float32>(j.n));

				NkScene t;
				t.Init(cfg);
				t.AjouterSysteme("fixe", NkPhaseSysteme::NK_PAS_FIXE, [](NkScene &, float32 dt, void *) { j.fixes++; j.dtFixe = dt; });
				t.AjouterSysteme("trame", NkPhaseSysteme::NK_TRAME, [](NkScene &, float32 dt, void *) { j.trames++; j.dtTrame = dt; });
				t.Pas(1.f / 30.f);
				Temoin(j.fixes == 2 && j.trames == 1 && math::NkAbs(j.dtFixe - 1.f / 60.f) < 1.0e-6f && math::NkAbs(j.dtTrame - 1.f / 30.f) < 1.0e-6f,
					   "(j2) 2 pas fixes (1/60) et 1 trame (1/30), sans physique", static_cast<float32>(j.fixes));

				j = Journal{};
				NkScene u;
				u.Init(cfg);
				const uint32 off = u.AjouterSysteme("off", NkPhaseSysteme::NK_TRAME, A);
				u.ActiverSysteme(off, false);
				j.aRetirer = u.AjouterSysteme("seul", NkPhaseSysteme::NK_TRAME, [](NkScene &sc, float32, void *) {
					j.trace[j.n++] = 'r';
					sc.RetirerSysteme(j.aRetirer);
				});
				u.AjouterSysteme("apres", NkPhaseSysteme::NK_TRAME, C);
				u.Pas(1.f / 60.f);
				u.Pas(1.f / 60.f);
				// Trame 1 : r (qui se retire) puis c. Trame 2 : c seul. Jamais a.
				Temoin(std::strcmp(j.trace, "rcc") == 0 && u.NbSystemes() == 2u, "(j3) desactive muet ; retrait en cours de route sans saut",
					   static_cast<float32>(j.n));
				if (std::strcmp(j.trace, "rcc") != 0) {
					std::printf("    trace : %s\n", j.trace);
				}

				// (j4) contacts
				auto Chute = [&](NkScene &sc, float32 hauteur, ecs::NkEntityId &balle, ecs::NkEntityId &sol, ecs::NkEntityId *zone) {
					NkSceneConfig c2;
					c2.physique = true;
					sc.Init(c2);
					sol = sc.Creer("Sol", NkVec2f(0.f, -0.5f));
					NkCollisionneur2D cs;
					cs.demiTaille = NkVec2f(10.f, 0.5f);
					sc.Monde().Add<NkCollisionneur2D>(sol, cs);
					NkCorps2D ks;
					ks.type = NkTypeCorps::NK_STATIQUE;
					sc.AjouterCorps(sol, ks);
					if (zone != nullptr) {
						*zone = sc.Creer("Zone", NkVec2f(0.f, 3.f));
						NkCollisionneur2D cz;
						cz.demiTaille = NkVec2f(2.f, 0.5f);
						cz.declencheur = true;
						sc.Monde().Add<NkCollisionneur2D>(*zone, cz);
						NkCorps2D kz;
						kz.type = NkTypeCorps::NK_STATIQUE;
						sc.AjouterCorps(*zone, kz);
					}
					balle = sc.Creer("Balle", NkVec2f(hauteur > 50.f ? 40.f : 0.f, hauteur));
					NkCollisionneur2D cb;
					cb.forme = NkForme2D::NK_CERCLE;
					cb.rayon = 0.25f;
					sc.Monde().Add<NkCollisionneur2D>(balle, cb);
					NkCorps2D kb;
					sc.AjouterCorps(balle, kb);
				};
				int32 debuts = 0, autres = 0;
				{
					NkScene w;
					ecs::NkEntityId balle, sol;
					Chute(w, 5.f, balle, sol, nullptr);
					for (int32 k = 0; k < 150; ++k) {
						w.Pas(1.f / 60.f);
						for (uint32 i = 0; i < w.Contacts().Size(); ++i) {
							const NkContact2D &ct = w.Contacts()[i];
							const bool paire = (ct.a == balle && ct.b == sol) || (ct.a == sol && ct.b == balle);
							if (paire && ct.phase == NkPhaseContact::NK_DEBUT && !ct.declencheur) {
								++debuts;
							} else {
								++autres;
							}
						}
					}
				}
				Temoin(debuts >= 1 && autres == 0, "(j4) balle sur le sol : un DEBUT balle/sol, en entites", static_cast<float32>(debuts));
				int32 rien = 0;
				{
					NkScene w;
					ecs::NkEntityId balle, sol;
					Chute(w, 100.f, balle, sol, nullptr);
					for (int32 k = 0; k < 60; ++k) {
						w.Pas(1.f / 60.f);
						rien += static_cast<int32>(w.Contacts().Size());
					}
				}
				Temoin(rien == 0, "(j4n) balle dans le vide : aucun contact", static_cast<float32>(rien));
				int32 entre = -1, sort = -1, pas = 0;
				{
					NkScene w;
					ecs::NkEntityId balle, sol, zone;
					Chute(w, 5.f, balle, sol, &zone);
					for (int32 k = 0; k < 150; ++k, ++pas) {
						w.Pas(1.f / 60.f);
						for (uint32 i = 0; i < w.Contacts().Size(); ++i) {
							const NkContact2D &ct = w.Contacts()[i];
							if (ct.declencheur && ct.a == zone && ct.b == balle) {
								if (ct.phase == NkPhaseContact::NK_DEBUT && entre < 0) {
									entre = pas;
								} else if (ct.phase == NkPhaseContact::NK_FIN && sort < 0) {
									sort = pas;
								}
							}
						}
					}
				}
				Temoin(entre >= 0 && sort > entre, "(j5) zone traversee : DEBUT puis FIN (pas de l'entree)", static_cast<float32>(entre));
			}

			// (n1) .. (n10)
			NkUnkenyBancAnimateur(&Temoin);

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC UNKENY REUSSI" : "BANC UNKENY EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "");
			// Le banc du jeu et celui de la structure de scene impriment chacun
			// leur PROPRE bilan : celui-ci reste comparable d'une version a
			// l'autre (37 temoins au 29/09/2026).
			const int32 echecs = gEchecs;
			const int32 jeu = NkUnkenyLancerBancJeu();
			const int32 structure = NkUnkenyLancerBancStructure();
			return (echecs == 0 && jeu == 0 && structure == 0) ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
