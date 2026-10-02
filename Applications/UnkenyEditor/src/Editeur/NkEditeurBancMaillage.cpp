//
// NkEditeurBancMaillage.cpp
// =============================================================================
// Description :
//   Le banc du MAILLAGE 2D DANS L'EDITEUR (2026-10-02, R31 ; NkEditeurMaillage.h),
//   joue sur LA VRAIE TRAME (NkEditeurBancTrame.h : clics, glisser, touches comme
//   depuis l'OS), et ses CAPTURES HORS ECRAN (--captures-maillage=DOSSIER).
//   Compte a part (« BANC MAILLAGE EDITEUR »).
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (n1)  « Ajouter un composant > Maillage 2D » sur une entite a sprite : le
//         maillage nait du contour de l'image, le sprite est MASQUE ; Ctrl+Z rend
//         l'entite d'avant (pas de maillage, sprite visible) ; Ctrl+Y le remet
//   (n2)  le clic droit sur un sprite propose « Creer un maillage 2D depuis le
//         sprite » ; le cliquer cree le maillage ET ouvre sa fenetre (un onglet de
//         document) ; « Ajouter un composant » propose les trois sources
//   (n3)  la carte « Maillage 2D » est dans les Details (pastille Rendu)
//   (n4)  dans la fenetre : un clic CHOISIT un sommet, le glisser le DEPLACE de ce
//         qu'a fait la souris ; Ctrl+Z le rend ; (n4n) contre-epreuve : le meme
//         deplacement SANS retenir -> le temoin de Ctrl+Z voit ECHEC
//   (n5)  l'outil Ajouter (A) pose un sommet au clic ; Suppr le retire
//   (n6)  un cadre de choix sur la moitie droite, puis P : DEUX parties ; le
//         bouton « Rigide » du panneau donne sa physique a la partie
//   (n7)  JOUER : la partie rigide TOMBE (le maillage suit), la partie sans
//         physique reste ; ARRETER : tout revient au repos, aucun corps libre
//   (n8)  enregistrer la scene, la rouvrir : le maillage est le meme
//   (n9)  l'asset : « Enregistrer comme asset » ecrit le .nkmesh2d ; « Utiliser »
//         le pose sur une autre entite ; le poser du Contenu cree une entite
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Editeur/NkEditeurMaillage.h"
#include "Editeur/NkEditeurTrame.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

			/// LA GELEE du Dernier Feu, dessinee au programme : un dome vert d'eau,
			/// son reflet, deux yeux, un sourire ; alpha nul autour (le contour que le
			/// maillage doit trouver).
			struct NkImageGelee {
					static constexpr int32 W = 160, H = 128;
					uint8 px[W * H * 4];
					NkImageGelee() {
						for (int32 y = 0; y < H; ++y) {
							for (int32 x = 0; x < W; ++x) {
								const float32 u = (static_cast<float32>(x) + 0.5f) / static_cast<float32>(W) * 2.f - 1.f;
								const float32 v = (static_cast<float32>(y) + 0.5f) / static_cast<float32>(H) * 2.f - 1.f;
								const float32 e = v < 0.f ? 2.2f : 4.f;
								const float32 d = std::pow(std::fabs(u / 0.94f), e) + std::pow(std::fabs(v / 0.92f), e);
								uint8 *p = px + (y * W + x) * 4;
								if (d > 1.f) {
									p[0] = p[1] = p[2] = 0u;
									p[3] = 0u;
									continue;
								}
								const float32 t = (v + 1.f) * 0.5f;
								float32 r = 110.f + (40.f - 110.f) * t, g = 228.f + (150.f - 228.f) * t, b = 170.f + (120.f - 170.f) * t;
								if (d > 0.78f) {
									const float32 k = 1.f - (d - 0.78f) / 0.22f * 0.38f;
									r *= k;
									g *= k;
									b *= k;
								}
								// Le reflet.
								const float32 hx = (u + 0.38f) / 0.24f, hy = (v + 0.48f) / 0.12f;
								if (hx * hx + hy * hy < 1.f) {
									const float32 k = 0.65f * (1.f - (hx * hx + hy * hy));
									r += (255.f - r) * k;
									g += (255.f - g) * k;
									b += (255.f - b) * k;
								}
								// Les yeux, leur eclat ; le sourire.
								for (int32 o = -1; o <= 1; o += 2) {
									const float32 ex = u - 0.27f * static_cast<float32>(o), ey = v + 0.05f;
									if (ex * ex + ey * ey < 0.11f * 0.11f) {
										r = 22.f;
										g = 44.f;
										b = 46.f;
										const float32 gx = ex - 0.035f, gy = ey + 0.035f;
										if (gx * gx + gy * gy < 0.035f * 0.035f) {
											r = g = b = 250.f;
										}
									}
								}
								const float32 sx = u, sy = v - 0.12f;
								const float32 rs = std::sqrt(sx * sx + sy * sy);
								if (sy > 0.04f && std::fabs(rs - 0.17f) < 0.022f) {
									r = 22.f;
									g = 44.f;
									b = 46.f;
								}
								p[0] = static_cast<uint8>(r < 0.f ? 0.f : (r > 255.f ? 255.f : r));
								p[1] = static_cast<uint8>(g < 0.f ? 0.f : (g > 255.f ? 255.f : g));
								p[2] = static_cast<uint8>(b < 0.f ? 0.f : (b > 255.f ? 255.f : b));
								p[3] = d > 0.96f ? static_cast<uint8>((1.f - d) / 0.04f * 255.f) : 255u;
							}
						}
					}
			};

			/// La scene de demonstration de l'editeur, SANS ses acteurs (le sol et les
			/// murs restent) : le maillage y est seul.
			void SceneNue(NkEditeurModele &m) {
				NkEditeurNouvelleScene(m);
				NkVector<ecs::NkEntityId> ids;
				m.scene.Entites(ids);
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(ids[i]);
					const bool garder = e != nullptr && (std::strcmp(e->nom, "Sol") == 0 || std::strncmp(e->nom, "Mur", 3) == 0);
					if (!garder && m.scene.Monde().IsAlive(ids[i])) {
						m.scene.Detruire(ids[i]);
					}
				}
				NkEditeurOublierHistorique(m);
			}

			/// La Gelee : une entite a sprite texture (1,6 x 1,28 m), au-dessus du sol.
			ecs::NkEntityId Gelee(NkEditeurModele &m, const NkVec2f &pos) {
				uint32 tex = m.textures.Trouver("Gelee.png");
				if (tex == 0u) {
					// NkTextures2D garde SA copie des pixels : l'image peut partir.
					NkImageGelee *image = memory::NkGetDefaultAllocator().New<NkImageGelee>();
					if (image != nullptr) {
						tex = m.textures.Creer(image->px, NkImageGelee::W, NkImageGelee::H, "Gelee.png");
						memory::NkGetDefaultAllocator().Delete(image);
					}
				}
				const ecs::NkEntityId e = m.scene.Creer("Gelee", pos);
				NkSprite2D s;
				s.texId = tex;
				s.taille = NkVec2f(1.6f, 1.28f);
				m.scene.Monde().Add<NkSprite2D>(e, s);
				return e;
			}

			NkMaillage2D *Ml(NkEditeurModele &m, ecs::NkEntityId e) {
				return m.scene.Monde().IsAlive(e) ? m.scene.Monde().Get<NkMaillage2D>(e) : nullptr;
			}

			const NkEditeurInterface::NkLigneMenuPeinte *LigneMenu(NkEditeurInterface &ui, const char *debut) {
				for (uint32 i = 0; i < ui.menuLignes.Size(); ++i) {
					if (std::strncmp(ui.menuLignes[i].libelle.CStr(), debut, std::strlen(debut)) == 0) {
						return &ui.menuLignes[i];
					}
				}
				return nullptr;
			}

			/// La moitie droite de la gelee : ses sommets choisis par un cadre, puis P.
			void ChoisirDroiteEtFaire(NkEditeurBancTrame &T, NkDocMaillage &d, const NkMaillage2D &ml, float32 seuilX) {
				NkVec2f mn(1.0e9f, 1.0e9f), mx(-1.0e9f, -1.0e9f);
				for (uint32 i = 0; i < ml.nbSommets; ++i) {
					if (ml.positions[i].x < seuilX) {
						continue;
					}
					mn = NkVec2f(math::NkMin(mn.x, ml.positions[i].x), math::NkMin(mn.y, ml.positions[i].y));
					mx = NkVec2f(math::NkMax(mx.x, ml.positions[i].x), math::NkMax(mx.y, ml.positions[i].y));
				}
				const NkVec2f a = NkEditeurMaillageVersEcran(d, NkVec2f(seuilX - 0.01f, mx.y + 0.1f));
				const NkVec2f b = NkEditeurMaillageVersEcran(d, NkVec2f(mx.x + 0.1f, mn.y - 0.1f));
				T.Glisser(a.x, a.y, b.x, b.y, 4);
				T.Touche(nkgui::NkGuiKey::P);
			}

			/// Les y monde de la partie `k` (min).
			float32 YMin(NkEditeurModele &m, ecs::NkEntityId e, uint32 k) {
				const NkMaillage2D *ml = Ml(m, e);
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(e);
				if (ml == nullptr || t == nullptr) {
					return 0.f;
				}
				NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
				NkMaillagePositionsMonde(m.scene, e, *t, *ml, w);
				float32 y = 1.0e9f;
				for (uint32 i = 0; i < ml->nbSommets; ++i) {
					if (ml->partieSommet[i] == k) {
						y = math::NkMin(y, w[i].y);
					}
				}
				return y;
			}

			uint32 NbCorps(NkEditeurModele &m) {
				return m.scene.MondePhysique() != nullptr ? static_cast<uint32>(m.scene.MondePhysique()->Bodies().Size()) : 0u;
			}

			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strcmp(e.nom, nom) == 0) {
						t = id;
					}
				});
				return t;
			}
		} // namespace

		// =====================================================================
		int32 NkEditeurLancerBancMaillage() {
			gE = 0;
			gR = 0;
			std::printf("\n== BANC MAILLAGE EDITEUR (R31) ==\n");
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_banc_maillage/";
			NkDirectory::Delete(racine.CStr(), true);
			NkDirectory::CreateRecursive((racine + "Contenu").CStr());
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			SceneNue(m);
			m.chemin = racine + "Banc.nkscene";
			NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			NkEditeurInterface &ui = T.Ui();
			ui.sonsMuets = true;
			T.Trame();

			// ── (n1) Ajouter un composant > Maillage 2D ──
			const ecs::NkEntityId g = Gelee(m, NkVec2f(0.f, 0.f));
			{
				const bool ajoute = NkEditeurAjouterComposant(m, g, NkComposantEditeur::NK_MAILLAGE);
				const NkMaillage2D *ml = Ml(m, g);
				const NkSprite2D *s = m.scene.Monde().Get<NkSprite2D>(g);
				const bool okCree = ajoute && ml != nullptr && ml->nbTriangles > 0u && NkTrianglesRetournes2D(*ml) == 0u &&
									NkMaillageCoherent2D(*ml) && s != nullptr && !s->visible && ml->texId == s->texId;
				Temoin(okCree, "(n1) Ajouter un composant > Maillage 2D : depuis le sprite, sprite masque",
					   ml != nullptr ? static_cast<float32>(ml->nbSommets) : 0.f);
				NkEditeurAnnuler(m);
				const ecs::NkEntityId g2 = ParNom(m.scene, "Gelee");
				const bool annule = Ml(m, g2) == nullptr && m.scene.Monde().Get<NkSprite2D>(g2) != nullptr &&
									m.scene.Monde().Get<NkSprite2D>(g2)->visible;
				NkEditeurRefaire(m);
				const ecs::NkEntityId g3 = ParNom(m.scene, "Gelee");
				Temoin(annule && Ml(m, g3) != nullptr, "(n1) Ctrl+Z : plus de maillage, sprite visible ; Ctrl+Y le remet", 0.f);
				NkEditeurRetirerComposant(m, g3, NkComposantEditeur::NK_MAILLAGE);
				NkEditeurOublierHistorique(m);
			}
			ecs::NkEntityId gel = ParNom(m.scene, "Gelee");

			// ── (n2) Le clic droit, et « Ajouter un composant » ──
			{
				m.selection = gel;
				m.aSelection = true;
				{
					NkEditeurCadre c = T.Cadre();
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, nkgui::NkRect{400.f, 300.f, 10.f, 10.f});
				}
				T.Trame();
				const bool troisSources = LigneMenu(ui, "Maillage 2D depuis le sprite") != nullptr && LigneMenu(ui, "Maillage 2D vide") != nullptr;
				T.Fermer();
				NkEditeurCadre c = T.Cadre();
				NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_ENTITE, nkgui::NkRect{400.f, 300.f, 10.f, 10.f});
				T.Trame();
				const NkEditeurInterface::NkLigneMenuPeinte *l = LigneMenu(ui, "Créer un maillage 2D depuis le sprite");
				const bool propose = l != nullptr;
				if (l != nullptr) {
					const nkgui::NkRect r = l->r;
					T.Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				}
				T.Fermer();
				gel = ParNom(m.scene, "Gelee");
				const NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
				Temoin(propose && Ml(m, gel) != nullptr && d != nullptr && NkEditeurCibleMaillage(m, *d) == gel,
					   "(n2) clic droit > Creer un maillage 2D depuis le sprite : cree, fenetre ouverte", 0.f);
				Temoin(troisSources, "(n2) Ajouter un composant propose les sources du maillage", 0.f);
			}

			// ── (n3) La carte des Details ──
			{
				const NkDocumentOuvert page = NkEditeurDocumentActif(ui);
				NkEditeurActiverDocument(m, ui, NkDocScene());
				m.selection = gel;
				m.aSelection = true;
				ui.ongletDroite = 0;
				ui.detailsCategorie = 4;
				T.Trame();
				T.Trame();
				const bool carte = (ui.detailsCartesDessinees & (1u << static_cast<uint32>(NkCarteEditeur::NK_MAILLAGE))) != 0u;
				Temoin(carte, "(n3) la carte « Maillage 2D » est dans les Details (pastille Rendu)", 0.f);
				ui.detailsCategorie = 0;
				NkEditeurActiverDocument(m, ui, page);
			}

			// ── (n4) Choisir et glisser un sommet dans la fenetre ──
			NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
			{
				T.Trame();
				NkMaillage2D *ml = Ml(m, gel);
				// Le sommet le plus a gauche (sur le bord : rien ne le cache).
				uint32 s = 0;
				for (uint32 i = 1; ml != nullptr && i < ml->nbSommets; ++i) {
					s = ml->positions[i].x < ml->positions[s].x ? i : s;
				}
				const NkVec2f avant = ml != nullptr ? ml->positions[s] : NkVec2f(0.f, 0.f);
				const NkVec2f uvAvant = ml != nullptr ? ml->uvs[s] : NkVec2f(0.f, 0.f);
				const NkVec2f e0 = d != nullptr ? NkEditeurMaillageVersEcran(*d, avant) : NkVec2f(0.f, 0.f);
				T.Clic(0, e0.x, e0.y);
				const bool choisi = d != nullptr && d->choisis[s] != 0u;
				ui.aimant = false; // le deplacement exact, sans coller
				T.Glisser(e0.x, e0.y, e0.x - 40.f, e0.y, 5);
				ml = Ml(m, gel);
				const float32 attendu = d != nullptr ? 40.f / d->zoom : 0.f;
				const float32 bouge = ml != nullptr ? avant.x - ml->positions[s].x : 0.f;
				Temoin(choisi && std::fabs(bouge - attendu) < 0.02f * attendu + 1.0e-4f,
					   "(n4) un clic choisit un sommet, le glisser le deplace de la souris", bouge);
				// « UV collees » (defaut) : la texture reste a sa place -- l'UV du sommet
				// suit sa nouvelle position (vers la gauche de l'image : u diminue).
				Temoin(ml != nullptr && ml->uvs[s].x < uvAvant.x - 0.01f, "(n4) UV collees : l'UV suit le sommet, la texture ne glisse pas",
					   ml != nullptr ? uvAvant.x - ml->uvs[s].x : 0.f);
				T.Touche(nkgui::NkGuiKey::Z, true);
				// Ctrl+Z REFAIT les entites (NkScene::Restaurer) : la poignee change,
				// l'identite non (la fenetre la suit par son uid).
				gel = ParNom(m.scene, "Gelee");
				ml = Ml(m, gel);
				Temoin(ml != nullptr && std::fabs(ml->positions[s].x - avant.x) < 1.0e-5f, "(n4) Ctrl+Z dans la fenetre : le sommet revient",
					   ml != nullptr ? ml->positions[s].x - avant.x : 1.f);
				// (n4n) La contre-epreuve : le MEME deplacement, ecrit sans retenir.
				NkEditeurOublierHistorique(m);
				if (ml != nullptr) {
					ml->positions[s].x -= attendu;
				}
				NkEditeurAnnuler(m);
				gel = ParNom(m.scene, "Gelee");
				ml = Ml(m, gel);
				const bool revenu = ml != nullptr && std::fabs(ml->positions[s].x - avant.x) < 1.0e-5f;
				Temoin(ml != nullptr && !revenu, "(n4n) sans retenir -> le temoin de Ctrl+Z voit ECHEC", 0.f);
				if (ml != nullptr) {
					ml->positions[s] = avant;
				}
			}

			// ── (n5) Ajouter (A), Suppr ──
			{
				NkMaillage2D *ml = Ml(m, gel);
				const uint32 n0 = ml != nullptr ? ml->nbSommets : 0u;
				T.Touche(nkgui::NkGuiKey::A);
				const NkVec2f p = d != nullptr ? NkEditeurMaillageVersEcran(*d, NkVec2f(-0.13f, 0.21f)) : NkVec2f(0.f, 0.f);
				T.Clic(0, p.x, p.y);
				ml = Ml(m, gel);
				const uint32 n1 = ml != nullptr ? ml->nbSommets : 0u;
				T.Touche(nkgui::NkGuiKey::Q);
				T.Touche(nkgui::NkGuiKey::Delete);
				ml = Ml(m, gel);
				const uint32 n2 = ml != nullptr ? ml->nbSommets : 0u;
				Temoin(n1 == n0 + 1u && n2 == n0 && ml != nullptr && NkTrianglesRetournes2D(*ml) == 0u,
					   "(n5) Ajouter (A) pose un sommet au clic, Suppr le retire", static_cast<float32>(n1));
			}

			// ── (n6) Un cadre sur la moitie droite, P ; « Rigide » ──
			{
				NkMaillage2D *ml = Ml(m, gel);
				if (ml != nullptr && d != nullptr) {
					ChoisirDroiteEtFaire(T, *d, *ml, 0.3f);
				}
				ml = Ml(m, gel);
				const bool deux = ml != nullptr && ml->nbParties == 2u && NkMaillageCoherent2D(*ml);
				T.Trame();
				if (d != nullptr) {
					d->partie = 1;
					T.Trame();
					const nkgui::NkRect r = d->physique[1];
					T.Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				}
				ml = Ml(m, gel);
				Temoin(deux, "(n6) un cadre de choix puis P : deux parties", ml != nullptr ? static_cast<float32>(ml->nbParties) : 0.f);
				Temoin(ml != nullptr && ml->nbParties == 2u && ml->parties[1].physique == static_cast<uint8>(NkPhysiquePartie2D::NK_RIGIDE),
					   "(n6) le bouton « Rigide » du panneau donne sa physique a la partie", 0.f);
			}

			// ── (n7) Jouer : la partie tombe ; Arreter : au repos ──
			{
				const uint32 corps0 = NbCorps(m);
				const float32 y0 = YMin(m, gel, 1), yg0 = YMin(m, gel, 0);
				NkEditeurJouer(m);
				for (int32 k = 0; k < 90; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				T.Trame(); // la fenetre en jeu (lecture seule)
				const float32 y1 = YMin(m, gel, 1), yg1 = YMin(m, gel, 0);
				const NkMaillage2D *ml = Ml(m, gel);
				std::printf("  (jouer) partie rigide y %.3f -> %.3f ; corps %.3f -> %.3f\n", static_cast<double>(y0), static_cast<double>(y1),
							static_cast<double>(yg0), static_cast<double>(yg1));
				Temoin(ml != nullptr && ml->construit && y1 < y0 - 2.f && std::fabs(yg1 - yg0) < 1.0e-4f,
					   "(n7) JOUER : la partie rigide tombe (le maillage suit), le corps reste", y0 - y1);
				NkEditeurArreter(m);
				gel = ParNom(m.scene, "Gelee");
				ml = Ml(m, gel);
				Temoin(ml != nullptr && !ml->construit && std::fabs(YMin(m, gel, 1) - y0) < 1.0e-4f && NbCorps(m) == corps0,
					   "(n7) ARRETER : au repos, aucun corps libre ne reste", static_cast<float32>(NbCorps(m)) - static_cast<float32>(corps0));
			}

			// ── (n8) Enregistrer, rouvrir ──
			{
				const NkMaillage2D *ml = Ml(m, gel);
				const uint32 ns = ml != nullptr ? ml->nbSommets : 0u, np = ml != nullptr ? ml->nbParties : 0u;
				NkVec2f p0 = ml != nullptr ? ml->positions[3] : NkVec2f(0.f, 0.f);
				const bool ecrit = NkEditeurSauver(m);
				const bool lu = NkEditeurOuvrir(m);
				gel = ParNom(m.scene, "Gelee");
				ml = Ml(m, gel);
				Temoin(ecrit && lu && ml != nullptr && ml->nbSommets == ns && ml->nbParties == np && ml->positions[3].x == p0.x &&
						   ml->parties[1].physique == static_cast<uint8>(NkPhysiquePartie2D::NK_RIGIDE) && ml->texId != 0u,
					   "(n8) enregistrer la scene, la rouvrir : le maillage est le meme", static_cast<float32>(ns));
			}

			// ── (n9) L'asset .nkmesh2d ──
			{
				const NkString chemin = NkEditeurMaillageEnregistrerAsset(m, gel);
				const bool existe = !chemin.Empty() && NkFile::Exists(chemin.CStr());
				const ecs::NkEntityId autre = NkEditeurCreerEntite(m, "Autre", NkVec2f(3.f, 0.f));
				const bool utilise = NkEditeurMaillageUtiliserAsset(m, autre, chemin.CStr());
				const NkMaillage2D *a = Ml(m, gel);
				const NkMaillage2D *b = Ml(m, autre);
				const ecs::NkEntityId pose = NkEditeurPoserMaillageAsset(m, chemin.CStr(), NkVec2f(-3.f, 0.f));
				Temoin(existe && utilise && a != nullptr && b != nullptr && b->nbSommets == a->nbSommets && b->source[0] != '\0' &&
						   Ml(m, pose) != nullptr,
					   "(n9) l'asset .nkmesh2d : ecrit, utilise sur une entite, pose du Contenu", static_cast<float32>(chemin.Length()));
			}

			NkEditeurFermerDocumentDevant(m, ui);
			al.Delete(pt);
			al.Delete(pm);
			NkDirectory::Delete(racine.CStr(), true);
			std::printf("== MAILLAGE EDITEUR : %d / %d ==\n", gR, gR + gE);
			return gE == 0 ? 0 : 1;
		}

		// =====================================================================
		// LES CAPTURES HORS ECRAN
		// =====================================================================
		namespace {
			bool Png(NkEditeurBancTrame &T, const char *chemin) {
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

		int32 NkEditeurCapturesMaillage(const char *dossier) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkDirectory::CreateRecursive(dossier);
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_captures_maillage/";
			NkDirectory::Delete(racine.CStr(), true);
			NkDirectory::CreateRecursive((racine + "Contenu").CStr());
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			SceneNue(m);
			m.chemin = racine + "Gelee.nkscene";
			NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			T.W = 1600.f;
			T.H = 900.f;
			T.pctx->Init(1600, 900);
			NkEditeurInterface &ui = T.Ui();
			ui.sonsMuets = true;
			int32 erreurs = 0;
			auto Trames = [&](int32 n) {
				for (int32 k = 0; k < n; ++k) {
					T.Trame();
				}
			};
			auto Capture = [&](const char *nom) {
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(2);
				erreurs += Png(T, NkString::Format("%s/%s", dossier, nom).CStr()) ? 0 : 1;
			};
			// La gelee, au-dessus du sol (y = -4), avec son maillage.
			const ecs::NkEntityId g0 = Gelee(m, NkVec2f(0.f, -1.6f));
			NkEditeurCreerMaillage(m, g0, NkSourceMaillage::NK_SPRITE, 7);
			ecs::NkEntityId gel = ParNom(m.scene, "Gelee");
			m.selection = gel;
			m.aSelection = true;
			m.scene.Camera().PoserCentre(NkVec2f(0.f, -2.3f));
			m.scene.Camera().PoserZoom(110.f);
			ui.cadrageEnAttente = false;
			ui.ongletDroite = 0;
			ui.detailsCategorie = 4; // la pastille Rendu : la carte Maillage 2D en vue
			// La carte ENTIERE : le tiroir replie, la carte du sprite (masque) repliee.
			ui.voirTiroir = false;
			ui.cartesRepliees |= 1u << static_cast<uint32>(NkCarteEditeur::NK_SPRITE);
			Trames(4);
			Capture("01_details_carte_maillage.png");
			ui.voirTiroir = true;
			ui.cartesRepliees &= ~(1u << static_cast<uint32>(NkCarteEditeur::NK_SPRITE));

			// ── La fenetre d'edition ──
			NkEditeurOuvrirMaillage(m, ui, gel);
			Trames(3);
			Capture("02_fenetre_edition_sommets_triangles.png");
			NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
			// Trois parties : le lobe gauche (molle), le lobe droit (rigide), le corps.
			if (d != nullptr) {
				NkMaillage2D *ml = Ml(m, gel);
				ChoisirDroiteEtFaire(T, *d, *ml, 0.38f);
				ml = Ml(m, gel);
				// Le lobe gauche : le miroir.
				NkVec2f mn(1.0e9f, 1.0e9f), mx(-1.0e9f, -1.0e9f);
				for (uint32 i = 0; i < ml->nbSommets; ++i) {
					if (ml->positions[i].x > -0.38f) {
						continue;
					}
					mn = NkVec2f(math::NkMin(mn.x, ml->positions[i].x), math::NkMin(mn.y, ml->positions[i].y));
					mx = NkVec2f(math::NkMax(mx.x, ml->positions[i].x), math::NkMax(mx.y, ml->positions[i].y));
				}
				const NkVec2f a = NkEditeurMaillageVersEcran(*d, NkVec2f(mn.x - 0.1f, mx.y + 0.1f));
				const NkVec2f b = NkEditeurMaillageVersEcran(*d, NkVec2f(-0.37f, mn.y - 0.1f));
				T.Glisser(a.x, a.y, b.x, b.y, 4);
				T.Touche(nkgui::NkGuiKey::P);
				ml = Ml(m, gel);
				if (ml != nullptr && ml->nbParties == 3u) {
					std::snprintf(ml->parties[1].nom, sizeof(ml->parties[1].nom), "Lobe droit");
					std::snprintf(ml->parties[2].nom, sizeof(ml->parties[2].nom), "Lobe gauche");
					NkEditeurMaillagePhysique(m, gel, 1, NkPhysiquePartie2D::NK_RIGIDE);
					NkEditeurMaillagePhysique(m, gel, 2, NkPhysiquePartie2D::NK_MOLLE);
					const int32 l = NkEditeurMaillageLier(m, gel, 2, 0, NkGenreLienParties2D::NK_ELASTIQUE);
					d->partie = 1;
					d->lien = -1;
					(void)l;
				}
			}
			Trames(3);
			Capture("03_parties_colorees_rigide.png");
			if (d != nullptr) {
				d->partie = 2;
				d->lien = 0;
				d->texture = false;
			}
			Trames(3);
			Capture("04_partie_molle_lien_elastique_sans_texture.png");
			if (d != nullptr) {
				d->texture = true;
			}
			// ── JOUER : le lobe droit tombe, le gauche tremble, le corps reste ──
			NkEditeurActiverDocument(m, ui, NkDocScene());
			Trames(2);
			NkEditeurJouer(m);
			for (int32 k = 0; k < 40; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
				T.Trame();
			}
			Capture("05_jouer_la_partie_rigide_tombe.png");
			for (int32 k = 0; k < 50; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
				T.Trame();
			}
			Capture("06_jouer_posee_sur_le_sol.png");
			// La fenetre d'edition EN JEU : le maillage tel que la physique le tient.
			if (d != nullptr) {
				NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_MAILLAGE, d->id));
			}
			Trames(2);
			Capture("07_fenetre_en_jeu_lecture_seule.png");
			NkEditeurArreter(m);
			NkEditeurActiverDocument(m, ui, NkDocScene());
			// ── Le clic droit sur un sprite ──
			const ecs::NkEntityId autre = Gelee(m, NkVec2f(3.2f, -1.6f));
			m.selection = autre;
			m.aSelection = true;
			Trames(2);
			{
				NkEditeurCadre c = T.Cadre();
				const NkVec2f e = m.scene.Camera().MondeVersEcran(NkVec2f(3.2f, -1.6f));
				NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_ENTITE, nkgui::NkRect{e.x, e.y, 2.f, 2.f});
				Trames(1);
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				T.Trame();
				erreurs += Png(T, NkString::Format("%s/%s", dossier, "08_clic_droit_creer_maillage.png").CStr()) ? 0 : 1;
				T.Fermer();
			}
			// « Ajouter un composant » : les trois sources du maillage (section Rendu).
			{
				NkEditeurCadre c = T.Cadre();
				NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, ui.detailsAjouter.w > 0.f ? ui.detailsAjouter : nkgui::NkRect{1300.f, 200.f, 2.f, 2.f});
				Trames(1);
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				T.Trame();
				erreurs += Png(T, NkString::Format("%s/%s", dossier, "09_ajouter_un_composant_maillage.png").CStr()) ? 0 : 1;
				T.Fermer();
			}
			NkEditeurFermerDocumentDevant(m, ui);
			al.Delete(pt);
			al.Delete(pm);
			NkDirectory::Delete(racine.CStr(), true);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
