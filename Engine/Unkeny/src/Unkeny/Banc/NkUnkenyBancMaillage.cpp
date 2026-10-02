//
// NkUnkenyBancMaillage.cpp
// =============================================================================
// Description :
//   Le banc du MAILLAGE 2D (2026-10-02, R31 ; Maillage/NkUnkenyMaillage.h et
//   NkUnkenyMaillagePhysique.h). Compte A PART (« BANC MAILLAGE ») : les bancs
//   d'avant gardent leurs comptes.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (m1)  depuis un SPRITE (un disque echancre, l'alpha a 0 ailleurs) : entre 12
//         et 128 sommets, au moins n-2 triangles, AUCUN triangle retourne,
//         coherent, l'aire a +/- 12 % de celle des pixels opaques, l'echancrure
//         reste VIDE, les UV dans la region du sprite ; (m1n) contre-epreuve : un
//         triangle inverse a la main -> le temoin des retournes voit ECHEC
//   (m2)  la DENSITE regle le nombre de sommets (3 < 10) ; une image opaque donne
//         son rectangle ; une image transparente : refus
//   (m3)  depuis une FORME (etoile) : couleur du remplissage, creux vide
//   (m4)  le DESSIN : la texture deformee (le rouge de l'image au centre, le fond
//         dans l'echancrure) ; deplacer un sommet deplace l'image ; l'ordre des
//         parties ; (m4n) sans texture, les couleurs de sommet
//   (m5)  enregistrer / relire / enregistrer la scene : le MEME JSON a l'octet,
//         le maillage identique ; (m5b) l'asset .nkmesh2d aller-retour ; un autre
//         format refuse ; (m5c) un PREFAB porte le maillage a son instance ;
//         (m5d) le type Mesh2D au point de passage unique (.nkmesh2d)
//   (m6)  FAIRE UNE PARTIE : 2 parties, les coutures DEDOUBLEES (voisines),
//         triangles et aire gardes, coherent ; (m6n) contre-epreuve : un sommet de
//         couture rendu a l'autre partie -> incoherent vu
//   (m7)  la PHYSIQUE : la partie droite RIGIDE tombe sur le sol et s'y pose, la
//         gauche (sans physique) reste ; le maillage SUIT (ses sommets = la pose
//         du corps) ; (m7n) sans physique, rien ne tombe
//   (m8)  les LIENS : soudee a la gauche, la droite ne tombe pas ; en PIVOT elle
//         tourne autour de la couture (le point de couture reste) ; une soudure
//         de rupture faible CASSE et la partie tombe
//   (m9)  une partie MOLLE tombe, se pose, ses sommets SONT ses particules (aucun
//         NaN), et le rendu des corps mous ne la dessine pas
//   (m10) Restaurer (Arreter) : le maillage revient au repos, plus un corps libre
//   (m11) ajouter / couper / retirer des sommets : coherent, aucun retourne
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKMemory/NKMemory.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "Unkeny/Maillage/NkUnkenyMaillage.h"
#include "Unkeny/Maillage/NkUnkenyMaillageFichier.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyRenduParticules.h"
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

			struct NkSceneTas {
					NkScene *s = nullptr;
					NkSceneTas(bool physique, bool particules) {
						s = memory::NkGetDefaultAllocator().New<NkScene>();
						NkSceneConfig cfg;
						cfg.physique = physique;
						cfg.particules = particules;
						s->Init(cfg);
						if (particules && s->Particules() != nullptr) {
							s->Particules()->reglages.limites.actif = false; // le sol est un corps
						}
					}
					~NkSceneTas() {
						memory::NkGetDefaultAllocator().Delete(s);
					}
					NkScene &operator*() noexcept {
						return *s;
					}
			};

			template <typename T> struct NkTas {
					T *p = nullptr;
					NkTas() {
						p = memory::NkGetDefaultAllocator().New<T>();
					}
					~NkTas() {
						memory::NkGetDefaultAllocator().Delete(p);
					}
					T &operator*() noexcept {
						return *p;
					}
					T *operator->() noexcept {
						return p;
					}
			};

			/// L'image d'essai : un DISQUE rouge de rayon 40 px centre dans 96 x 96, dont
			/// on a retire une ECHANCRURE (le quart droit, un « pac-man »). Alpha 0 ailleurs.
			struct NkImageDisque {
					uint8 px[96 * 96 * 4];
					int32 opaques = 0;
					NkImageDisque() {
						for (int32 y = 0; y < 96; ++y) {
							for (int32 x = 0; x < 96; ++x) {
								const float32 dx = static_cast<float32>(x) + 0.5f - 48.f, dy = static_cast<float32>(y) + 0.5f - 48.f;
								const bool disque = dx * dx + dy * dy <= 40.f * 40.f;
								const bool echancre = dx > 0.f && (dy < 0.f ? -dy : dy) < dx * 0.6f; // un coin vers la droite
								const bool plein = disque && !echancre;
								uint8 *p = px + (y * 96 + x) * 4;
								p[0] = 230u;
								p[1] = 40u;
								p[2] = 30u;
								p[3] = plein ? 255u : 0u;
								opaques += plein ? 1 : 0;
							}
						}
					}
			};

			/// Le sprite : 96 px = 1,92 m (50 px/m), centre.
			NkSprite2D SpriteDisque(uint32 tex) {
				NkSprite2D s;
				s.taille = NkVec2f(1.92f, 1.92f);
				s.texId = tex;
				return s;
			}

			float32 Aire(const NkMaillage2D &m) {
				float32 a = 0.f;
				for (uint32 t = 0; t < m.nbTriangles; ++t) {
					a += NkAireTriangle2D(m, t);
				}
				return a;
			}

			/// Pas fixes de la scene pendant `secondes`.
			void Simuler(NkScene &s, float32 secondes) {
				const int32 n = static_cast<int32>(secondes * 60.f + 0.5f);
				for (int32 i = 0; i < n; ++i) {
					s.Pas(1.f / 60.f);
				}
			}

			/// Le sol : un rectangle STATIQUE de 10 x 0,4 m dont le dessus est a y = 0.
			ecs::NkEntityId Sol(NkScene &s) {
				NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_RECTANGLE);
				f.taille = NkVec2f(10.f, 0.4f);
				const ecs::NkEntityId e = NkPoserForme2D(s, f, NkVec2f(0.f, -0.2f), "Sol", true, -1);
				NkCorps2D c;
				c.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(e, c);
				return e;
			}

			/// Une plaque de 2 x 1 m (grille 8 x 4), coupee en DEUX parties : x < 0
			/// (partie 0) et x > 0 (partie 1).
			void Plaque(NkMaillage2D &m) {
				NkMaillageRectangle2D(m, NkVec2f(2.f, 1.f), NkVec2f(0.5f, 0.5f), 8, 4);
				uint8 choix[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					choix[i] = m.positions[i].x >= -1.0e-4f ? 1u : 0u;
				}
				NkMaillageFairePartie2D(m, choix, "Droite");
			}

			/// Les sommets MONDE de la partie `k` : leur y minimal et maximal.
			void BornesPartie(const NkScene &s, const NkTransform2D &t, const NkMaillage2D &m, uint32 k, float32 &ymin, float32 &ymax) {
				NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
				NkMaillagePositionsMonde(s, t, m, w);
				ymin = 1.0e9f;
				ymax = -1.0e9f;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					if (m.partieSommet[i] == k) {
						ymin = w[i].y < ymin ? w[i].y : ymin;
						ymax = w[i].y > ymax ? w[i].y : ymax;
					}
				}
			}

			uint32 NbCorps(NkScene &s) {
				return s.MondePhysique() != nullptr ? static_cast<uint32>(s.MondePhysique()->Bodies().Size()) : 0u;
			}

			// =================================================================
			void BancFabrication() {
				NkTas<NkImageDisque> img;
				NkTas<NkMaillage2D> pm;
				NkMaillage2D &m = *pm;
				const NkSprite2D s = SpriteDisque(0x554B0001u);
				NkOptionsMaillage2D o;
				o.densite = 6;
				const bool ok = NkMaillageDepuisSprite2D(m, s, img->px, 96, 96, o);
				const char *pourquoi = "";
				const bool coherent = NkMaillageCoherent2D(m, &pourquoi);
				std::printf("  (sprite) %u sommets, %u triangles, %u parties\n", m.nbSommets, m.nbTriangles, m.nbParties);
				Temoin(ok && m.nbSommets >= 12u && m.nbSommets <= NK_MAILLAGE2D_SOMMETS_MAX,
					   "(m1) depuis un sprite : entre 12 et 128 sommets", static_cast<float32>(m.nbSommets));
				Temoin(m.nbTriangles + 2u >= m.nbSommets, "(m1) au moins n - 2 triangles", static_cast<float32>(m.nbTriangles));
				Temoin(NkTrianglesRetournes2D(m) == 0u, "(m1) AUCUN triangle retourne", static_cast<float32>(NkTrianglesRetournes2D(m)));
				Temoin(coherent, "(m1) coherent (indices, parties)", 0.f);
				const float32 aireOpaque = static_cast<float32>(img->opaques) / (50.f * 50.f);
				const float32 aire = Aire(m);
				Temoin(aire > aireOpaque * 0.88f && aire < aireOpaque * 1.12f, "(m1) l'aire a +/- 12 % des pixels opaques (m2)", aire / aireOpaque);
				// L'echancrure (a droite du centre) est VIDE ; le flanc gauche est plein.
				Temoin(NkTriangleSous2D(m, NkVec2f(0.5f, 0.f)) < 0 && NkTriangleSous2D(m, NkVec2f(-0.5f, 0.f)) >= 0,
					   "(m1) l'echancrure reste VIDE, le flanc gauche plein", 0.f);
				bool uvOk = true;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					uvOk = uvOk && m.uvs[i].x >= -1.0e-3f && m.uvs[i].x <= 1.001f && m.uvs[i].y >= -1.0e-3f && m.uvs[i].y <= 1.001f;
				}
				Temoin(uvOk && m.texId == s.texId, "(m1) UV dans la region du sprite, sa texture reprise", 0.f);
				// (m1n) La contre-epreuve : un triangle inverse a la main.
				{
					NkTas<NkMaillage2D> pc;
					*pc = m;
					const uint8 x = pc->triangles[1];
					pc->triangles[1] = pc->triangles[2];
					pc->triangles[2] = x;
					Temoin(NkTrianglesRetournes2D(*pc) == 1u, "(m1n) un triangle inverse -> le temoin des retournes voit ECHEC",
						   static_cast<float32>(NkTrianglesRetournes2D(*pc)));
				}
				// (m2) La densite.
				{
					NkTas<NkMaillage2D> a, b;
					NkOptionsMaillage2D o3 = o, o10 = o;
					o3.densite = 3;
					o10.densite = 10;
					NkMaillageDepuisSprite2D(*a, s, img->px, 96, 96, o3);
					NkMaillageDepuisSprite2D(*b, s, img->px, 96, 96, o10);
					std::printf("  (densite) 3 -> %u sommets, 10 -> %u sommets\n", a->nbSommets, b->nbSommets);
					Temoin(a->nbSommets < b->nbSommets && NkTrianglesRetournes2D(*b) == 0u, "(m2) la densite regle le nombre de sommets",
						   static_cast<float32>(b->nbSommets) - static_cast<float32>(a->nbSommets));
				}
				{
					uint8 plein[16 * 16 * 4];
					uint8 vide[16 * 16 * 4];
					for (int32 i = 0; i < 16 * 16; ++i) {
						plein[i * 4] = plein[i * 4 + 1] = plein[i * 4 + 2] = plein[i * 4 + 3] = 255u;
						vide[i * 4] = vide[i * 4 + 1] = vide[i * 4 + 2] = vide[i * 4 + 3] = 0u;
					}
					NkTas<NkMaillage2D> a, b;
					NkSprite2D sp;
					sp.taille = NkVec2f(2.f, 1.f);
					const bool okPlein = NkMaillageDepuisSprite2D(*a, sp, plein, 16, 16, o);
					const bool okVide = NkMaillageDepuisSprite2D(*b, sp, vide, 16, 16, o);
					Temoin(okPlein && Aire(*a) > 1.99f && Aire(*a) < 2.01f && !okVide,
						   "(m2) une image opaque donne son rectangle, une transparente est refusee", Aire(*a));
				}
				// (m3) Depuis une forme : l'etoile.
				{
					NkRenduForme2D f = NkFormeParDefaut(NkGenreForme2D::NK_ETOILE);
					f.taille = NkVec2f(3.f, 3.f);
					f.rayonInterieur = 0.4f;
					f.remplissage = 0x20C040FFu;
					NkTas<NkMaillage2D> e;
					const bool okE = NkMaillageDepuisForme2D(*e, f, o);
					const NkVec2f creux(-0.588f, 0.809f);
					Temoin(okE && NkTrianglesRetournes2D(*e) == 0u && e->couleurs[0] == 0x20C040FFu && NkTriangleSous2D(*e, creux) < 0 &&
							   NkTriangleSous2D(*e, NkVec2f(0.f, 0.f)) >= 0,
						   "(m3) depuis une etoile : sa couleur, son creux VIDE, aucun retourne", static_cast<float32>(e->nbSommets));
					NkRenduForme2D l = NkFormeParDefaut(NkGenreForme2D::NK_LIGNE);
					Temoin(!NkMaillageDepuisForme2D(*e, l, o), "(m3) une ligne ouverte : refus", 0.f);
				}
			}

			// =================================================================
			/// Rend la scene dans 200 x 200 px, 50 px/m, (0,0) au centre ; fond noir.
			struct NkRendu {
					nkgui::NkGuiContext ctx;
					nkgui::NkGuiDrawListRaster raster;
					void Rendre(NkScene &s, NkTextures2D *tex) {
						s.Camera().PoserViseur(nkgui::NkRect{0.f, 0.f, 200.f, 200.f});
						s.Camera().PoserCentre(NkVec2f(0.f, 0.f));
						s.Camera().PoserZoom(50.f);
						ctx.Init(200, 200);
						ctx.BeginFrame(1.f / 60.f);
						NkDessinerScene(ctx.dl, s);
						ctx.EndFrame();
						raster.Init(200, 200);
						raster.Effacer(0x000000FFu);
						if (tex != nullptr) {
							for (uint32 k = 0; k < tex->Nombre(); ++k) {
								const uint32 id = NkTextures2D::kPremierId + k;
								int32 w = 0, h = 0;
								if (tex->Taille(id, w, h) && tex->Pixels(id) != nullptr) {
									raster.PoserTexture(id, tex->Pixels(id), w, h, 4);
								}
							}
						}
						raster.Rasteriser(ctx.dl);
					}
					uint32 Px(float32 x, float32 y) const {
						int32 px = static_cast<int32>(100.f + 50.f * x);
						int32 py = static_cast<int32>(100.f - 50.f * y);
						px = px < 0 ? 0 : (px > 199 ? 199 : px);
						py = py < 0 ? 0 : (py > 199 ? 199 : py);
						const uint8 *p = raster.Pixels() + (py * raster.Largeur() + px) * 4;
						return (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) | (static_cast<uint32>(p[2]) << 8) | 0xFFu;
					}
			};

			bool Rougeatre(uint32 px) {
				return ((px >> 24) & 0xFFu) > 150u && ((px >> 16) & 0xFFu) < 100u;
			}
			bool Noir(uint32 px) {
				return ((px >> 24) & 0xFFu) < 30u && ((px >> 16) & 0xFFu) < 30u && ((px >> 8) & 0xFFu) < 30u;
			}

			void BancDessin() {
				NkTas<NkImageDisque> img;
				NkTas<NkTextures2D> tex;
				const uint32 id = tex->Creer(img->px, 96, 96, "disque");
				NkSceneTas s(false, false);
				const ecs::NkEntityId e = (*s).Creer("Maillage", NkVec2f(0.f, 0.f));
				{
					NkTas<NkMaillage2D> m;
					NkOptionsMaillage2D o;
					NkMaillageDepuisSprite2D(*m, SpriteDisque(id), img->px, 96, 96, o);
					// Des couleurs de sommet BLANCHES : la texture telle quelle.
					for (uint32 i = 0; i < m->nbSommets; ++i) {
						m->couleurs[i] = 0xFFFFFFFFu;
					}
					(*s).Monde().Add<NkMaillage2D>(e, *m);
				}
				NkTas<NkRendu> r;
				r->Rendre(*s, &*tex);
				Temoin(Rougeatre(r->Px(-0.3f, 0.f)) && Noir(r->Px(0.6f, 0.f)) && Noir(r->Px(1.5f, 1.5f)),
					   "(m4) la texture deformee : rouge sur le disque, fond dans l'echancrure", 0.f);
				// Deplacer le sommet le plus a gauche de 0,6 m vers la gauche : l'image s'etire.
				{
					NkMaillage2D *m = (*s).Monde().Get<NkMaillage2D>(e);
					uint32 g = 0;
					for (uint32 i = 1; i < m->nbSommets; ++i) {
						g = m->positions[i].x < m->positions[g].x ? i : g;
					}
					const NkVec2f avant = m->positions[g];
					m->positions[g].x -= 0.6f;
					r->Rendre(*s, &*tex);
					const bool etire = Rougeatre(r->Px(avant.x - 0.25f, avant.y));
					m->positions[g] = avant;
					r->Rendre(*s, &*tex);
					Temoin(etire && Noir(r->Px(avant.x - 0.25f, avant.y)), "(m4) deplacer un sommet deplace l'image (et la rend)", 0.f);
				}
				// (m4n) Sans texture : les couleurs de sommet, et l'ORDRE des parties.
				{
					NkSceneTas s2(false, false);
					const ecs::NkEntityId e2 = (*s2).Creer("Plaque", NkVec2f(0.f, 0.f));
					NkTas<NkMaillage2D> m;
					Plaque(*m);
					for (uint32 i = 0; i < m->nbSommets; ++i) {
						m->couleurs[i] = m->partieSommet[i] == 1u ? 0x00FF00FFu : 0x0000FFFFu;
					}
					// La partie droite par-dessus la gauche, decalee de 0,5 m a gauche.
					for (uint32 i = 0; i < m->nbSommets; ++i) {
						if (m->partieSommet[i] == 1u) {
							m->positions[i].x -= 0.5f;
						}
					}
					m->parties[1].ordre = 5;
					(*s2).Monde().Add<NkMaillage2D>(e2, *m);
					r->Rendre(*s2, nullptr);
					const uint32 dessus = r->Px(-0.25f, 0.f);
					// L'ordre inverse : la gauche (bleue) passe devant.
					(*s2).Monde().Get<NkMaillage2D>(e2)->parties[1].ordre = -5;
					r->Rendre(*s2, nullptr);
					const uint32 dessous = r->Px(-0.25f, 0.f);
					Temoin(((dessus >> 16) & 0xFFu) > 200u && ((dessous >> 8) & 0xFFu) > 200u,
						   "(m4n) sans texture : couleurs de sommet ; l'ordre des parties decide", 0.f);
				}
			}

			// =================================================================
			void BancFichier() {
				NkTas<NkImageDisque> img;
				NkTas<NkTextures2D> tex;
				const uint32 id = tex->Creer(img->px, 96, 96, "disque.png");
				NkRessourcesScene res;
				res.textures = &*tex;
				NkSceneTas s(false, false);
				const ecs::NkEntityId e = (*s).Creer("Maillage", NkVec2f(1.f, 2.f));
				{
					NkTas<NkMaillage2D> m;
					NkOptionsMaillage2D o;
					NkMaillageDepuisSprite2D(*m, SpriteDisque(id), img->px, 96, 96, o);
					uint8 choix[NK_MAILLAGE2D_SOMMETS_MAX] = {};
					for (uint32 i = 0; i < m->nbSommets; ++i) {
						choix[i] = m->positions[i].y > 0.1f ? 1u : 0u;
					}
					NkMaillageFairePartie2D(*m, choix, "Haut");
					m->parties[1].physique = static_cast<uint8>(NkPhysiquePartie2D::NK_RIGIDE);
					m->parties[1].masse = 2.5f;
					NkMaillageAjouterLien2D(*m, 0, 1, NkGenreLienParties2D::NK_PIVOT);
					m->liens[0].rupture = 40.f;
					m->couche = 3;
					(*s).Monde().Add<NkMaillage2D>(e, *m);
				}
				NkString j1, j2;
				NkSauverSceneJSON(*s, j1, res);
				NkSceneTas s2(false, false);
				NkString err;
				const bool lu = NkChargerSceneJSON(*s2, j1.View(), res, &err);
				NkSauverSceneJSON(*s2, j2, res);
				Temoin(lu && j1 == j2, "(m5) enregistrer / relire / enregistrer : le MEME JSON", static_cast<float32>(j1.Length()));
				const NkMaillage2D *a = (*s).Monde().Get<NkMaillage2D>(e);
				const NkMaillage2D *b = nullptr;
				(*s2).Monde().Query<NkMaillage2D>().ForEach([&b](ecs::NkEntityId, NkMaillage2D &x) { b = &x; });
				const bool memes = b != nullptr && a->nbSommets == b->nbSommets && a->nbTriangles == b->nbTriangles &&
								   std::memcmp(a->positions, b->positions, sizeof(a->positions)) == 0 &&
								   std::memcmp(a->triangles, b->triangles, sizeof(a->triangles)) == 0 && b->nbParties == 2u &&
								   b->parties[1].physique == static_cast<uint8>(NkPhysiquePartie2D::NK_RIGIDE) && b->parties[1].masse == 2.5f &&
								   b->nbLiens == 1u && b->liens[0].rupture == 40.f && b->texId == id && b->couche == 3;
				Temoin(memes, "(m5) le maillage relu est le meme (sommets, parties, liens, texture)", 0.f);
				// (m5b) L'asset .nkmesh2d.
				{
					NkString js;
					NkMaillage2DVersJSON(*a, js, res);
					NkTas<NkMaillage2D> c;
					const bool okA = NkMaillage2DDepuisJSON(*c, js.CStr(), *s, res, &err);
					const bool pareil = okA && c->nbSommets == a->nbSommets && std::memcmp(c->uvs, a->uvs, sizeof(a->uvs)) == 0 &&
										c->texId == id && c->nbParties == 2u && std::strcmp(c->parties[1].nom, "Haut") == 0;
					Temoin(pareil, "(m5b) l'asset .nkmesh2d : aller-retour identique", static_cast<float32>(js.Length()));
					NkTas<NkMaillage2D> d;
					Temoin(!NkMaillage2DDepuisJSON(*d, "{\"format\": \"unkeny.prefab\", \"version\": 1}", *s, res, &err),
						   "(m5b) un autre format est REFUSE (l'en-tete est la verite)", 0.f);
				}
				// (m5c) Le prefab.
				{
					NkTas<NkPrefabs2D> prefabs;
					const uint32 p = prefabs->Creer(*s, e, "MaillagePrefab");
					const ecs::NkEntityId inst = prefabs->Instancier(*s, p, NkVec2f(5.f, 0.f));
					const NkMaillage2D *mi = (*s).Monde().Get<NkMaillage2D>(inst);
					Temoin(mi != nullptr && mi->nbSommets == a->nbSommets && mi->nbParties == 2u, "(m5c) un PREFAB porte le maillage a son instance",
						   mi != nullptr ? static_cast<float32>(mi->nbSommets) : 0.f);
				}
				// (m5d) Le type, au point de passage unique.
				Temoin(std::strcmp(NkAssetExtensionFor(NkAssetType::Mesh2D), "nkmesh2d") == 0 &&
						   NkAssetTypeFromExtension(".NKMESH2D") == NkAssetType::Mesh2D &&
						   NkAssetTypeFromExtension("nkmesh") == NkAssetType::StaticMesh,
					   "(m5d) Mesh2D <-> .nkmesh2d (et .nkmesh reste le StaticMesh 3D)", static_cast<float32>(NkAssetType::Mesh2D));
			}

			// =================================================================
			void BancParties() {
				NkTas<NkMaillage2D> pm;
				NkMaillage2D &m = *pm;
				NkMaillageRectangle2D(m, NkVec2f(2.f, 1.f), NkVec2f(0.5f, 0.5f), 8, 4);
				const uint32 avantTri = m.nbTriangles, avantSom = m.nbSommets;
				const float32 avantAire = Aire(m);
				uint8 choix[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					choix[i] = m.positions[i].x >= -1.0e-4f ? 1u : 0u;
				}
				const int32 k = NkMaillageFairePartie2D(m, choix, "Droite");
				const char *pourquoi = "";
				const bool coh = NkMaillageCoherent2D(m, &pourquoi);
				Temoin(k == 1 && m.nbParties == 2u && coh, "(m6) FAIRE UNE PARTIE : 2 parties, coherent", static_cast<float32>(k));
				// La couture x = 0 : 5 sommets (grille 8 x 4) DEDOUBLES.
				Temoin(m.nbSommets == avantSom + 5u && NkPartiesVoisines2D(m, 0, 1), "(m6) la couture est dedoublee (5 sommets de plus)",
					   static_cast<float32>(m.nbSommets) - static_cast<float32>(avantSom));
				Temoin(m.nbTriangles == avantTri && math::NkAbs(Aire(m) - avantAire) < 1.0e-4f && NkTrianglesRetournes2D(m) == 0u,
					   "(m6) triangles et aire gardes, aucun retourne", Aire(m));
				// (m6n) La contre-epreuve : un sommet de couture rendu a l'autre partie.
				{
					NkTas<NkMaillage2D> c;
					*c = m;
					for (uint32 i = 0; i < c->nbSommets; ++i) {
						if (c->partieSommet[i] == 1u && c->positions[i].x < 1.0e-4f) {
							c->partieSommet[i] = 0u;
							break;
						}
					}
					Temoin(!NkMaillageCoherent2D(*c), "(m6n) une couture rompue -> le temoin de coherence voit ECHEC", 0.f);
				}
				// Retirer la partie : tout revient, les doubles se ressoudent.
				{
					NkTas<NkMaillage2D> c;
					*c = m;
					NkMaillageAjouterLien2D(*c, 0, 1, NkGenreLienParties2D::NK_RIGIDE);
					const bool okR = NkMaillageRetirerPartie2D(*c, 1);
					Temoin(okR && c->nbParties == 1u && c->nbSommets == avantSom && c->nbLiens == 0u && NkMaillageCoherent2D(*c),
						   "(m6) retirer la partie : les doubles se ressoudent, son lien part", static_cast<float32>(c->nbSommets));
				}
			}

			// =================================================================
			/// Une scene physique avec un sol et la plaque a y = 3 (sa partie droite
			/// `physique`), liee si `lien` >= 0.
			ecs::NkEntityId ScenePlaque(NkScene &s, NkPhysiquePartie2D droite, int32 lien, float32 rupture = 0.f) {
				Sol(s);
				const ecs::NkEntityId e = s.Creer("Plaque", NkVec2f(0.f, 3.f));
				NkTas<NkMaillage2D> m;
				Plaque(*m);
				m->parties[1].physique = static_cast<uint8>(droite);
				m->parties[1].masse = 2.f;
				if (lien >= 0) {
					NkMaillageAjouterLien2D(*m, 0, 1, static_cast<NkGenreLienParties2D>(lien));
					m->liens[0].rupture = rupture;
				}
				s.Monde().Add<NkMaillage2D>(e, *m);
				return e;
			}

			void BancPhysique() {
				// (m7) La partie droite RIGIDE tombe et se pose ; la gauche reste.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e = ScenePlaque(*s, NkPhysiquePartie2D::NK_RIGIDE, -1);
					const uint32 corpsAvant = NbCorps(*s);
					Simuler(*s, 2.5f);
					const NkTransform2D t = *(*s).Monde().Get<NkTransform2D>(e);
					const NkMaillage2D &m = *(*s).Monde().Get<NkMaillage2D>(e);
					float32 dMin = 0.f, dMax = 0.f, gMin = 0.f, gMax = 0.f;
					BornesPartie(*s, t, m, 1, dMin, dMax);
					BornesPartie(*s, t, m, 0, gMin, gMax);
					std::printf("  (physique) droite y = [%.3f, %.3f], gauche y = [%.3f, %.3f], %u corps libres\n", static_cast<double>(dMin),
								static_cast<double>(dMax), static_cast<double>(gMin), static_cast<double>(gMax), NbCorps(*s) - corpsAvant);
					Temoin(m.construit && NbCorps(*s) == corpsAvant + 1u, "(m7) un corps libre pour la partie rigide, construit au 1er pas",
						   static_cast<float32>(NbCorps(*s) - corpsAvant));
					Temoin(dMin > -0.08f && dMin < 0.12f, "(m7) la partie RIGIDE est tombee et POSEE sur le sol (y min ~ 0)", dMin);
					Temoin(math::NkAbs(gMin - 2.5f) < 1.0e-4f && math::NkAbs(gMax - 3.5f) < 1.0e-4f,
						   "(m7) la partie sans physique est restee (elle suit l'entite)", gMin);
					// Le maillage SUIT : chaque sommet de droite = la pose du corps.
					NkVec2f p;
					float32 a = 0.f;
					(*s).PoseCorps(m.parties[1].corps, p, a);
					NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
					NkMaillagePositionsMonde(*s, t, m, w);
					float32 ecart = 0.f;
					for (uint32 i = 0; i < m.nbSommets; ++i) {
						if (m.partieSommet[i] != 1u) {
							continue;
						}
						const NkVec2f l = m.positions[i] - m.parties[1].centre;
						const NkVec2f attendu(p.x + l.x * math::NkCos(a) - l.y * math::NkSin(a), p.y + l.x * math::NkSin(a) + l.y * math::NkCos(a));
						ecart = math::NkMax(ecart, math::NkAbs(attendu.x - w[i].x) + math::NkAbs(attendu.y - w[i].y));
					}
					Temoin(ecart < 1.0e-4f, "(m7) le maillage SUIT sa partie (sommets = pose du corps)", ecart);
				}
				// (m7n) Sans physique : rien ne tombe.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e = ScenePlaque(*s, NkPhysiquePartie2D::NK_AUCUNE, -1);
					const uint32 corpsAvant = NbCorps(*s);
					Simuler(*s, 1.f);
					float32 dMin = 0.f, dMax = 0.f;
					BornesPartie(*s, *(*s).Monde().Get<NkTransform2D>(e), *(*s).Monde().Get<NkMaillage2D>(e), 1, dMin, dMax);
					Temoin(math::NkAbs(dMin - 2.5f) < 1.0e-4f && NbCorps(*s) == corpsAvant, "(m7n) sans physique : rien ne tombe, aucun corps",
						   dMin);
				}
				// (m8) SOUDEE a la gauche : elle ne tombe pas.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e = ScenePlaque(*s, NkPhysiquePartie2D::NK_RIGIDE, static_cast<int32>(NkGenreLienParties2D::NK_RIGIDE));
					Simuler(*s, 2.f);
					float32 dMin = 0.f, dMax = 0.f;
					BornesPartie(*s, *(*s).Monde().Get<NkTransform2D>(e), *(*s).Monde().Get<NkMaillage2D>(e), 1, dMin, dMax);
					Temoin(dMin > 2.3f, "(m8) soudee a la partie fixe : elle ne tombe pas", dMin);
				}
				// (m8) En PIVOT : elle tourne autour de la couture, le point de couture reste.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e = ScenePlaque(*s, NkPhysiquePartie2D::NK_RIGIDE, static_cast<int32>(NkGenreLienParties2D::NK_PIVOT));
					Simuler(*s, 0.6f);
					const NkMaillage2D &m = *(*s).Monde().Get<NkMaillage2D>(e);
					NkVec2f p;
					float32 a = 0.f;
					(*s).PoseCorps(m.parties[1].corps, p, a);
					// Le milieu de la couture (0, 3) vu depuis le corps : il n'a pas bouge.
					const NkVec2f l = NkVec2f(0.f, 0.f) - m.parties[1].centre;
					const NkVec2f pivot(p.x + l.x * math::NkCos(a) - l.y * math::NkSin(a), p.y + l.x * math::NkSin(a) + l.y * math::NkCos(a));
					const float32 ecart = math::NkSqrt(pivot.x * pivot.x + (pivot.y - 3.f) * (pivot.y - 3.f));
					Temoin(math::NkAbs(a) > 0.3f && ecart < 0.05f, "(m8) en PIVOT : elle tourne, le point de couture reste", a);
				}
				// (m8) Une soudure de RUPTURE faible casse : la partie tombe.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e =
						ScenePlaque(*s, NkPhysiquePartie2D::NK_RIGIDE, static_cast<int32>(NkGenreLienParties2D::NK_RIGIDE), 5.f);
					Simuler(*s, 2.f);
					const NkMaillage2D &m = *(*s).Monde().Get<NkMaillage2D>(e);
					float32 dMin = 0.f, dMax = 0.f;
					BornesPartie(*s, *(*s).Monde().Get<NkTransform2D>(e), m, 1, dMin, dMax);
					Temoin(m.liens[0].casse && dMin < 0.2f, "(m8) une soudure de rupture faible (5 N) CASSE : la partie tombe", dMin);
				}
				// (m9) Une partie MOLLE.
				{
					NkSceneTas s(true, true);
					const ecs::NkEntityId e = ScenePlaque(*s, NkPhysiquePartie2D::NK_MOLLE, -1);
					(*s).Pas(1.f / 60.f);
					{
						const NkMaillage2D &m0 = *(*s).Monde().Get<NkMaillage2D>(e);
						std::printf("  (molle, 1er pas) construit %d, corps %u, corps mous %u, particules %u\n", m0.construit ? 1 : 0,
									m0.parties[1].corps, (*s).Particules() != nullptr ? static_cast<uint32>((*s).Particules()->corps.Size()) : 0u,
									(*s).Particules() != nullptr ? static_cast<uint32>((*s).Particules()->particules.Size()) : 0u);
					}
					Simuler(*s, 2.5f);
					const NkTransform2D t = *(*s).Monde().Get<NkTransform2D>(e);
					const NkMaillage2D &m = *(*s).Monde().Get<NkMaillage2D>(e);
					float32 dMin = 0.f, dMax = 0.f;
					BornesPartie(*s, t, m, 1, dMin, dMax);
					const physics::NkParticules2D *pw = (*s).Particules();
					const int32 ci = pw != nullptr ? pw->IndexCorps(m.parties[1].corps) : -1;
					bool fini = true, memes = ci >= 0;
					NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
					NkMaillagePositionsMonde(*s, t, m, w);
					uint32 r = 0;
					for (uint32 i = 0; i < m.nbSommets; ++i) {
						fini = fini && w[i].x == w[i].x && w[i].y == w[i].y;
						if (ci >= 0 && m.partieSommet[i] == 1u) {
							const NkVec2f q = pw->particules[pw->corps[static_cast<uint32>(ci)].debut + m.parties[1].premier + r].pos;
							memes = memes && q.x == w[i].x && q.y == w[i].y;
							++r;
						}
					}
					std::printf("  (molle) y = [%.3f, %.3f], %u particules\n", static_cast<double>(dMin), static_cast<double>(dMax),
								ci >= 0 ? pw->corps[static_cast<uint32>(ci)].nombre : 0u);
					Temoin(ci >= 0 && dMin > -0.15f && dMin < 0.25f && dMax > dMin + 0.3f, "(m9) la partie MOLLE est tombee et posee (sans s'aplatir)", dMin);
					Temoin(fini && memes, "(m9) ses sommets SONT ses particules (aucun NaN)", 0.f);
					Temoin(ci >= 0 && (pw->corps[static_cast<uint32>(ci)].utilisateur & NK_CORPS_MOU_DE_MAILLAGE) != 0u &&
							   (*s).EntiteDuCorpsMou(m.parties[1].corps) == e,
						   "(m9) sa matiere est MARQUEE (le rendu des corps mous ne la dessine pas)", 0.f);
				}
				// (m10) Restaurer : retour au repos, plus aucun corps libre.
				{
					NkSceneTas s(true, true);
					ScenePlaque(*s, NkPhysiquePartie2D::NK_RIGIDE, static_cast<int32>(NkGenreLienParties2D::NK_ELASTIQUE));
					NkTas<NkScene::NkPhoto> photo;
					(*s).Photographier(*photo);
					const uint32 corpsAvant = NbCorps(*s);
					Simuler(*s, 1.f);
					const uint32 enJeu = NbCorps(*s);
					(*s).Restaurer(*photo);
					const NkMaillage2D *m = nullptr;
					ecs::NkEntityId e;
					(*s).Monde().Query<NkMaillage2D>().ForEach([&](ecs::NkEntityId id, NkMaillage2D &x) {
						m = &x;
						e = id;
					});
					float32 dMin = 0.f, dMax = 0.f;
					if (m != nullptr) {
						BornesPartie(*s, *(*s).Monde().Get<NkTransform2D>(e), *m, 1, dMin, dMax);
					}
					Temoin(enJeu > corpsAvant && NbCorps(*s) == corpsAvant && m != nullptr && !m->construit &&
							   math::NkAbs(dMin - 2.5f) < 1.0e-4f,
						   "(m10) Restaurer : au repos, plus un corps libre (elastique compris)", static_cast<float32>(NbCorps(*s)));
				}
			}

			// =================================================================
			void BancEdition() {
				NkTas<NkMaillage2D> pm;
				NkMaillage2D &m = *pm;
				NkMaillageRectangle2D(m, NkVec2f(2.f, 1.f), NkVec2f(0.5f, 0.5f), 2, 1);
				const uint32 s0 = m.nbSommets, t0 = m.nbTriangles;
				const int32 dedans = NkMaillageAjouterSommet2D(m, NkVec2f(-0.4f, 0.1f));
				Temoin(dedans >= 0 && m.nbSommets == s0 + 1u && m.nbTriangles == t0 + 2u && NkTrianglesRetournes2D(m) == 0u,
					   "(m11) ajouter un sommet DEDANS : +1 sommet, +2 triangles", static_cast<float32>(m.nbTriangles));
				const float32 aireAvant = Aire(m);
				const int32 dehors = NkMaillageAjouterSommet2D(m, NkVec2f(0.f, 1.2f));
				Temoin(dehors >= 0 && Aire(m) > aireAvant && NkTrianglesRetournes2D(m) == 0u && NkMaillageCoherent2D(m),
					   "(m11) ajouter un sommet DEHORS : relie au bord, l'aire grandit", Aire(m) - aireAvant);
				const int32 milieu = NkMaillageCouperArete2D(m, m.triangles[0], m.triangles[1]);
				Temoin(milieu >= 0 && NkTrianglesRetournes2D(m) == 0u && NkMaillageCoherent2D(m), "(m11) couper une arete", 0.f);
				uint8 choix[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				choix[dedans >= 0 ? static_cast<uint32>(dedans) : 0u] = 1u;
				const float32 aire2 = Aire(m);
				const uint32 retires = NkMaillageRetirerSommets2D(m, choix);
				Temoin(retires == 1u && NkTrianglesRetournes2D(m) == 0u && NkMaillageCoherent2D(m) && math::NkAbs(Aire(m) - aire2) < 1.0e-3f,
					   "(m11) retirer un sommet INTERIEUR : le trou se rebouche", Aire(m));
				Temoin(NkMaillageTrianguler2D(m, true) && NkTrianglesRetournes2D(m) == 0u && math::NkAbs(Aire(m) - aire2) < 1.0e-3f,
					   "(m11) trianguler (Delaunay, la forme gardee)", Aire(m));
			}
		} // namespace

		int32 NkUnkenyLancerBancMaillage() {
			gE = 0;
			gR = 0;
			std::printf("\n== BANC MAILLAGE 2D (R31) ==\n");
			BancFabrication();
			BancDessin();
			BancFichier();
			BancParties();
			BancPhysique();
			BancEdition();
			std::printf("== MAILLAGE 2D : %d / %d ==\n", gR, gR + gE);
			return gE == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
