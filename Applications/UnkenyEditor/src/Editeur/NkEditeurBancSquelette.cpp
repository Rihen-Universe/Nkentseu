//
// NkEditeurBancSquelette.cpp
// =============================================================================
// LE BANC DU SQUELETTE 2D DANS L'EDITEUR (2026-10-02, R30), sur la VRAIE trame
// (NkEditeurBancTrame : la souris et le clavier passent par la page), et les
// CAPTURES HORS ECRAN (`--captures-squelette=DOSSIER`).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (e1)  Ajouter un composant > Squelette 2D : Humanoide -> 16 os, le maillage
//         cree depuis le sprite, les sommets ponderes (chaleur) ; Ctrl+Z l'enleve
//   (e2)  le menu propose les cinq modeles et le vide ; la carte « Squelette 2D »
//         est dans les Details (pastille Animation)
//   (e3)  outil Os : Maj + GLISSER depuis le bout de l'os choisi pose un os CHAINE
//         (son parent = le choisi) ; Ctrl+Z l'enleve
//   (e4)  outil IK : tirer la tete de la main (vraie souris) l'amene sur la cible,
//         le coude garde son sens ; Ctrl+Z rend la pose. CONTRE-EPREUVE (e4n) : la
//         meme pose ecrite SANS retenir -> le temoin de Ctrl+Z voit ECHEC
//   (e5)  outil Poids : clic sur une tete = l'os ; un coup de pinceau donne du
//         poids ; Ctrl+Z le reprend
//   (e6)  la FRISE : « Tous les os » met une piste par os et sa cle au curseur ;
//         une cle d'os changee dans la frise change le clip ; l'apercu ne laisse
//         aucune trace dans la pose
//   (e7)  JOUER : le clip d'os joue (la peau bouge) ; Arreter rend le repos
//   (e8)  la CHAINE MOLLE en Jouer : l'echarpe pend ; Arreter la rend
//   (e9)  « Enregistrer le squelette » : un .nkskel que NKAnima relit, marque 2D
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurDocuments.h"
#include "Editeur/NkEditeurMaillage.h"
#include "Editeur/NkEditeurPagesAnim.h"
#include "Editeur/NkEditeurSquelette.h"
#include "Editeur/NkEditeurTrame.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "NKPhysics/NkParticules2D.h"
#include "Unkeny/Anim/NkUnkenyProprietes.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using unkeny::NkSquelette2D;

		namespace {
			int32 gE = 0;
			int32 gR = 0;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			// =================================================================
			// LE PERSONNAGE : une silhouette ENCAPUCHONNEE de profil (tournee vers
			// la droite), cape, echarpe qui flotte derriere, une braise dans la main.
			// Aucun trait de visage (regle de Rihen) : l'ouverture de la capuche est
			// une ombre.
			// =================================================================
			struct NkImageBodofia {
					static constexpr int32 W = 176;
					static constexpr int32 H = 288;
					uint8 px[W * H * 4] = {};
			};

			bool DansPoly(float32 x, float32 y, const float32 *p, int32 n) {
				bool dedans = false;
				for (int32 i = 0, j = n - 1; i < n; j = i++) {
					const float32 xi = p[i * 2], yi = p[i * 2 + 1], xj = p[j * 2], yj = p[j * 2 + 1];
					if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi + 1e-9f) + xi)) {
						dedans = !dedans;
					}
				}
				return dedans;
			}

			bool DansEllipse(float32 x, float32 y, float32 cx, float32 cy, float32 rx, float32 ry) {
				const float32 dx = (x - cx) / rx, dy = (y - cy) / ry;
				return dx * dx + dy * dy <= 1.f;
			}

			void Peindre(NkImageBodofia &im) {
				static const float32 cape[] = {62, 96, 112, 96, 124, 150, 134, 226, 116, 232, 98, 226, 80, 234, 60, 226, 44, 230, 50, 150};
				static const float32 bras[] = {98, 104, 116, 104, 124, 188, 108, 192};
				static const float32 echarpe[] = {64, 90, 74, 104, 50, 128, 30, 152, 18, 146, 34, 120};
				static const float32 pointe[] = {104, 26, 132, 58, 110, 70};
				for (int32 y = 0; y < NkImageBodofia::H; ++y) {
					for (int32 x = 0; x < NkImageBodofia::W; ++x) {
						const float32 fx = static_cast<float32>(x) + 0.5f, fy = static_cast<float32>(y) + 0.5f;
						uint8 r = 0, g = 0, b = 0, a = 0;
						auto Poser = [&](uint8 rr, uint8 gg, uint8 bb) {
							r = rr;
							g = gg;
							b = bb;
							a = 255;
						};
						// Les jambes et les bottes (sous la cape).
						// Ecartees (20 px) : la peau de chaque jambe ne touche pas l'autre.
						const bool jambeAr = fx >= 56.f && fx <= 72.f && fy >= 222.f && fy <= 272.f;
						const bool jambeAv = fx >= 96.f && fx <= 112.f && fy >= 222.f && fy <= 272.f;
						const bool botteAr = fx >= 54.f && fx <= 84.f && fy >= 268.f && fy <= 286.f && !(fx > 74.f && fy < 276.f);
						const bool botteAv = fx >= 94.f && fx <= 126.f && fy >= 268.f && fy <= 286.f && !(fx > 114.f && fy < 276.f);
						if (jambeAr || jambeAv) {
							Poser(44, 38, 50);
						}
						if (botteAr || botteAv) {
							Poser(64, 44, 34);
						}
						// La cape : un drape sombre, plis verticaux, plus clair vers le haut.
						if (DansPoly(fx, fy, cape, 10)) {
							const float32 pli = 0.5f + 0.5f * std::sin(fx * 0.32f + fy * 0.03f);
							const float32 haut = 1.f - (fy - 96.f) / 160.f;
							Poser(static_cast<uint8>(66 + 18 * haut + 10 * pli), static_cast<uint8>(56 + 12 * haut + 6 * pli),
								  static_cast<uint8>(72 + 14 * haut + 8 * pli));
						}
						// L'echarpe qui flotte derriere (rouge braise).
						if (DansPoly(fx, fy, echarpe, 6) || DansEllipse(fx, fy, 88.f, 97.f, 27.f, 10.f)) {
							const float32 trame = (static_cast<int32>(fx + fy) % 6) < 3 ? 1.f : 0.86f;
							Poser(static_cast<uint8>(196 * trame), static_cast<uint8>(72 * trame), static_cast<uint8>(40 * trame));
						}
						// La capuche (et sa pointe vers l'avant).
						if (DansEllipse(fx, fy, 86.f, 50.f, 34.f, 40.f) || DansPoly(fx, fy, pointe, 3)) {
							const float32 lum = 1.f - (fy - 10.f) / 140.f;
							Poser(static_cast<uint8>(78 + 26 * lum), static_cast<uint8>(64 + 18 * lum), static_cast<uint8>(80 + 20 * lum));
						}
						// L'OUVERTURE : une ombre, rien d'autre.
						if (DansEllipse(fx, fy, 110.f, 60.f, 11.f, 17.f)) {
							Poser(18, 14, 22);
						}
						// Le bras de devant (la manche), la main gantee.
						if (DansPoly(fx, fy, bras, 4)) {
							Poser(52, 42, 58); // la manche, plus sombre que la cape
						}
						if (DansEllipse(fx, fy, 117.f, 196.f, 8.f, 9.f)) {
							Poser(98, 80, 70); // le gant
						}
						// La BRAISE dans la main : le dernier feu.
						const float32 dx = fx - 124.f, dy = fy - 210.f;
						const float32 d2 = dx * dx + dy * dy;
						if (d2 <= 64.f) {
							const float32 k = 1.f - d2 / 64.f;
							Poser(255, static_cast<uint8>(140 + 100 * k), static_cast<uint8>(40 + 120 * k));
						}
						uint8 *p = &im.px[(y * NkImageBodofia::W + x) * 4];
						p[0] = r;
						p[1] = g;
						p[2] = b;
						p[3] = a;
					}
				}
			}

			/// Le pixel (x, y) de l'image -> le repere de l'entite (sprite 0,99 x 1,62 m, centre).
			NkVec2f VersMetres(float32 x, float32 y) {
				return NkVec2f((x / static_cast<float32>(NkImageBodofia::W) - 0.5f) * 0.99f, (0.5f - y / static_cast<float32>(NkImageBodofia::H)) * 1.62f);
			}

			ecs::NkEntityId Bodofia(NkEditeurModele &m, const NkVec2f &pos) {
				uint32 tex = m.textures.Trouver("Bodofia.png");
				if (tex == 0u) {
					NkImageBodofia *im = memory::NkGetDefaultAllocator().New<NkImageBodofia>();
					if (im != nullptr) {
						Peindre(*im);
						tex = m.textures.Creer(im->px, NkImageBodofia::W, NkImageBodofia::H, "Bodofia.png");
						memory::NkGetDefaultAllocator().Delete(im);
					}
				}
				const ecs::NkEntityId e = m.scene.Creer("Bodofia", pos);
				NkSprite2D s;
				s.texId = tex;
				s.taille = NkVec2f(0.99f, 1.62f);
				m.scene.Monde().Add<NkSprite2D>(e, s);
				return e;
			}

			/// Le squelette de profil mis a la CAPE (pas a l'echarpe), et l'echarpe en
			/// trois os confies a une chaine molle. Poids par la chaleur.
			void SqueletteBodofia(NkEditeurModele &m, ecs::NkEntityId e) {
				NkSquelette2D *sq = m.scene.Monde().Get<NkSquelette2D>(e);
				NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(e);
				if (sq == nullptr || ml == nullptr) {
					return;
				}
				unkeny::NkSqueletteModele(*sq, anim::NkSkeleton2DTemplate::NK_HUMANOID_PROFILE, VersMetres(44.f, 288.f), VersMetres(132.f, 0.f));
				// Les os POSES SUR L'IMAGE (les retouches qu'on ferait a la main, outil Os) :
				// chaque jambe sur la sienne, le bras de devant dans sa manche.
				struct Pose {
						const char *os;
						float32 tx, ty, qx, qy; // tete et queue, en pixels de l'image
				};
				static const Pose kPoses[] = {
					{"Hanches", 84, 214, 84, 182},	  {"Torse", 84, 182, 86, 104},	  {"Cou", 86, 104, 88, 90},
					{"Tete", 88, 90, 96, 22},		  {"BrasG", 70, 110, 66, 152},	  {"AvantBrasG", 66, 152, 66, 190},
					{"MainG", 66, 190, 68, 204},	  {"BrasD", 104, 108, 113, 150},  {"AvantBrasD", 113, 150, 117, 188},
					{"MainD", 117, 188, 122, 206},	  {"CuisseG", 64, 214, 64, 246},  {"JambeG", 64, 246, 64, 272},
					{"PiedG", 64, 272, 80, 282},	  {"CuisseD", 104, 214, 104, 246}, {"JambeD", 104, 246, 104, 272},
					{"PiedD", 104, 272, 120, 282},
				};
				for (const Pose &p : kPoses) {
					const int32 j = unkeny::NkSqueletteTrouverOs(*sq, p.os);
					if (j >= 0) {
						unkeny::NkSqueletteReposTeteQueue(*sq, static_cast<uint32>(j), VersMetres(p.tx, p.ty), VersMetres(p.qx, p.qy));
					}
				}
				const int32 cou = unkeny::NkSqueletteTrouverOs(*sq, "Cou");
				const int32 e1 = unkeny::NkSqueletteAjouterOs(*sq, "Echarpe1", cou, VersMetres(70.f, 98.f), VersMetres(52.f, 120.f));
				const int32 e2 = unkeny::NkSqueletteAjouterOs(*sq, "Echarpe2", e1, VersMetres(52.f, 120.f), VersMetres(37.f, 135.f));
				unkeny::NkSqueletteAjouterOs(*sq, "Echarpe3", e2, VersMetres(37.f, 135.f), VersMetres(22.f, 149.f));
				unkeny::NkSqueletteAjouterChaine(*sq, static_cast<uint32>(e1), 3);
				sq->chaines[0].trainee = 6.f; // une echarpe legere : beaucoup de prise au vent
				unkeny::NkAutoPoidsChaleur2D(*sq, *ml);
			}

			/// La MARCHE de profil (boucle de 0,8 s) : cuisses et bras en opposition,
			/// genoux qui plient au passage, hanches qui montent.
			anim::NkAnimationClip ClipMarche(const NkSquelette2D &s0) {
				NkSquelette2D *s = memory::NkGetDefaultAllocator().New<NkSquelette2D>(s0);
				anim::NkAnimationClip c;
				c.name = "Marche";
				c.loop = true;
				c.fps = 30.f;
				struct Cle {
						const char *os;
						float32 v[5];
				};
				static const Cle kCles[] = {
					{"CuisseD", {0.45f, 0.05f, -0.40f, 0.05f, 0.45f}},	   {"CuisseG", {-0.40f, 0.05f, 0.45f, 0.05f, -0.40f}},
					{"JambeD", {-0.10f, -0.70f, -0.15f, -0.10f, -0.10f}}, {"JambeG", {-0.15f, -0.10f, -0.10f, -0.70f, -0.15f}},
					{"BrasD", {-0.35f, 0.f, 0.35f, 0.f, -0.35f}},		   {"BrasG", {0.35f, 0.f, -0.35f, 0.f, 0.35f}},
					{"AvantBrasD", {0.25f, 0.15f, 0.05f, 0.15f, 0.25f}},  {"AvantBrasG", {0.05f, 0.15f, 0.25f, 0.15f, 0.05f}},
					{"Torse", {-0.06f, -0.04f, -0.06f, -0.04f, -0.06f}},
				};
				const int32 hanches = unkeny::NkSqueletteTrouverOs(*s, "Hanches");
				for (uint32 k = 0; k < 5u; ++k) {
					const float32 t = 0.2f * static_cast<float32>(k);
					unkeny::NkSqueletteRetourRepos(*s);
					for (const Cle &x : kCles) {
						const int32 j = unkeny::NkSqueletteTrouverOs(*s, x.os);
						if (j >= 0) {
							s->os[j].pangle = s->os[j].angle + x.v[k];
						}
					}
					if (hanches >= 0) {
						s->os[hanches].py = s->os[hanches].y + ((k % 2u) == 1u ? 0.025f : 0.f);
					}
					uint8 choisis[unkeny::NK_SQUELETTE2D_OS_MAX] = {};
					for (uint32 j = 0; j < s->nbOs; ++j) {
						choisis[j] = std::strncmp(s->os[j].nom, "Echarpe", 7) == 0 ? 0u : 1u; // l'echarpe est MOLLE
					}
					unkeny::NkSqueletteCle(*s, c, t, choisis);
				}
				c.duration = 0.8f;
				memory::NkGetDefaultAllocator().Delete(s);
				return c;
			}

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

			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strcmp(e.nom, nom) == 0) {
						t = id;
					}
				});
				return t;
			}

			const NkEditeurInterface::NkLigneMenuPeinte *LigneMenu(NkEditeurInterface &ui, const char *debut) {
				for (uint32 i = 0; i < ui.menuLignes.Size(); ++i) {
					if (std::strncmp(ui.menuLignes[i].libelle.CStr(), debut, std::strlen(debut)) == 0) {
						return &ui.menuLignes[i];
					}
				}
				return nullptr;
			}

			NkSquelette2D *Sq(NkEditeurModele &m, ecs::NkEntityId e) {
				return m.scene.Monde().IsAlive(e) ? m.scene.Monde().Get<NkSquelette2D>(e) : nullptr;
			}

			/// La tete d'un os, en monde du maillage (pose ou repos).
			NkVec2f Tete(const NkSquelette2D &s, uint32 j, bool pose) {
				math::NkMat4f monde[unkeny::NK_SQUELETTE2D_OS_MAX];
				pose ? unkeny::NkSqueletteMondePose(s, monde) : unkeny::NkSqueletteMondeRepos(s, monde);
				return unkeny::NkSqueletteTete(monde, j);
			}

			NkVec2f Queue(const NkSquelette2D &s, uint32 j, bool pose) {
				math::NkMat4f monde[unkeny::NK_SQUELETTE2D_OS_MAX];
				pose ? unkeny::NkSqueletteMondePose(s, monde) : unkeny::NkSqueletteMondeRepos(s, monde);
				return unkeny::NkSqueletteQueue(s, monde, j);
			}

			float32 Dist(const NkVec2f &a, const NkVec2f &b) {
				return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
			}

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

		// =====================================================================
		int32 NkEditeurLancerBancSquelette() {
			gE = 0;
			gR = 0;
			std::printf("\n== BANC SQUELETTE EDITEUR (R30) ==\n");
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_banc_squelette/";
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

			// ── (e1) Ajouter un composant > Squelette 2D : Humanoide ──
			ecs::NkEntityId b = Bodofia(m, NkVec2f(0.f, -2.f));
			{
				const bool ajoute = NkEditeurAjouterComposant(m, b, NkComposantEditeur::NK_SQUELETTE);
				b = ParNom(m.scene, "Bodofia");
				const NkSquelette2D *s = Sq(m, b);
				const NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(b);
				const uint32 ponderes = ml != nullptr ? unkeny::NkSommetsPonderes2D(*ml) : 0u;
				Temoin(ajoute && s != nullptr && s->nbOs == 16u && ml != nullptr && ponderes == ml->nbSommets,
					   "(e1) Ajouter > Squelette 2D : 16 os, maillage du sprite, tout sommet pondere", static_cast<float32>(ponderes));
				NkEditeurAnnuler(m);
				b = ParNom(m.scene, "Bodofia");
				const bool sansOs = Sq(m, b) == nullptr && m.scene.Monde().Get<NkMaillage2D>(b) != nullptr;
				NkEditeurRefaire(m);
				b = ParNom(m.scene, "Bodofia");
				Temoin(sansOs && Sq(m, b) != nullptr, "(e1) Ctrl+Z enleve le squelette (le maillage reste) ; Ctrl+Y le rend", 0.f);
			}

			// ── (e2) Le menu et la carte ──
			{
				m.selection = b;
				m.aSelection = true;
				NkEditeurRetirerComposant(m, b, NkComposantEditeur::NK_SQUELETTE);
				{
					NkEditeurCadre c = T.Cadre();
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, nkgui::NkRect{400.f, 200.f, 10.f, 10.f});
				}
				T.Trame();
				// « Ajouter un composant » montre UNE ligne, un sous-menu : ses modeles.
				const bool sousMenu = LigneMenu(ui, "Squelette 2D (modèle de départ)") != nullptr;
				T.Fermer();
				{
					NkEditeurCadre c = T.Cadre();
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT_SQUELETTE, nkgui::NkRect{400.f, 200.f, 10.f, 10.f});
				}
				T.Trame();
				const bool cinq = sousMenu && LigneMenu(ui, "Squelette 2D : Humanoïde (de face)") != nullptr && LigneMenu(ui, "Squelette 2D : Quadrupède") != nullptr &&
								  LigneMenu(ui, "Squelette 2D : Oiseau") != nullptr && LigneMenu(ui, "Squelette 2D : Créature libre") != nullptr &&
								  LigneMenu(ui, "Squelette 2D : Humanoïde de profil") != nullptr && LigneMenu(ui, "Squelette 2D vide") != nullptr;
				// Le clic sur « profil » (la vraie trame) pose le modele et ouvre la fenetre.
				const NkEditeurInterface::NkLigneMenuPeinte *l = LigneMenu(ui, "Squelette 2D : Humanoïde de profil");
				if (l != nullptr) {
					const nkgui::NkRect r = l->r;
					T.Clic(0, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
				}
				T.Fermer();
				b = ParNom(m.scene, "Bodofia");
				const NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
				Temoin(cinq && Sq(m, b) != nullptr && unkeny::NkSqueletteTrouverOs(*Sq(m, b), "PiedD") >= 0 && d != nullptr && d->outil == NkOutilMaillage::NK_OS,
					   "(e2) le sous-menu : cinq modeles et le vide ; « profil » pose et ouvre l'outil Os", 0.f);
				const NkDocumentOuvert page = NkEditeurDocumentActif(ui);
				NkEditeurActiverDocument(m, ui, NkDocScene());
				ui.ongletDroite = 0;
				ui.detailsCategorie = 5;
				T.Trame();
				T.Trame();
				const bool carte = (ui.detailsCartesDessinees & (1u << static_cast<uint32>(NkCarteEditeur::NK_SQUELETTE))) != 0u;
				Temoin(carte, "(e2) la carte « Squelette 2D » est dans les Details (pastille Animation)", 0.f);
				ui.detailsCategorie = 0;
				NkEditeurActiverDocument(m, ui, page);
				NkEditeurOublierHistorique(m);
			}

			NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
			ui.aimant = false;
			// ── (e3) Os : glisser depuis le bout de l'os choisi -> un os CHAINE ──
			{
				T.Trame();
				NkSquelette2D *s = Sq(m, b);
				const int32 tete = s != nullptr ? unkeny::NkSqueletteTrouverOs(*s, "Tete") : -1;
				const uint32 avant = s != nullptr ? s->nbOs : 0u;
				bool chaine = false, annule = false;
				if (d != nullptr && tete >= 0) {
					d->outil = NkOutilMaillage::NK_OS;
					d->os = tete;
					T.Trame();
					const NkVec2f q = NkEditeurMaillageVersEcran(*d, Queue(*s, static_cast<uint32>(tete), false));
					T.pctx->input.shiftDown = true; // Maj : CHAINER depuis le bout
					T.Glisser(q.x, q.y, q.x + 40.f, q.y - 30.f, 6);
					T.pctx->input.shiftDown = false;
					s = Sq(m, b);
					chaine = s != nullptr && s->nbOs == avant + 1u && s->os[s->nbOs - 1u].parent == tete &&
							 Dist(Tete(*s, s->nbOs - 1u, false), Queue(*s, static_cast<uint32>(tete), false)) < 0.02f;
					T.Touche(nkgui::NkGuiKey::Z, true);
					b = ParNom(m.scene, "Bodofia");
					annule = Sq(m, b) != nullptr && Sq(m, b)->nbOs == avant;
				}
				Temoin(chaine, "(e3) outil Os : Maj+glisser depuis le bout de la Tete pose un os CHAINE", 0.f);
				Temoin(annule, "(e3) Ctrl+Z (dans la fenetre) l'enleve", 0.f);
			}

			// ── (e4) IK : tirer la tete de la main (vraie souris) ──
			{
				d = NkEditeurDocMaillageActif(ui);
				NkSquelette2D *s = Sq(m, b);
				const int32 main = s != nullptr ? unkeny::NkSqueletteTrouverOs(*s, "MainD") : -1;
				bool atteinte = false, sens = false, rendu = false;
				float32 ecart = -1.f;
				if (d != nullptr && main >= 0) {
					d->outil = NkOutilMaillage::NK_IK;
					d->coude = -1;
					T.Trame();
					const int32 avb = s->os[main].parent;
					const bool sensAvant = unkeny::NkSqueletteCoudePositif2D(*s, static_cast<uint32>(avb), NkVec2f(s->os[main].px, s->os[main].py));
					const NkVec2f h = Tete(*s, static_cast<uint32>(main), true);
					const NkVec2f cible = h + NkVec2f(0.10f, 0.16f);
					const NkVec2f a = NkEditeurMaillageVersEcran(*d, h), z = NkEditeurMaillageVersEcran(*d, cible);
					T.Glisser(a.x, a.y, z.x, z.y, 8);
					s = Sq(m, b);
					ecart = Dist(Tete(*s, static_cast<uint32>(main), true), cible);
					atteinte = ecart < 2.f / d->zoom;
					sens = unkeny::NkSqueletteCoudePositif2D(*s, static_cast<uint32>(avb), NkVec2f(s->os[main].px, s->os[main].py)) == sensAvant;
					T.Touche(nkgui::NkGuiKey::Z, true);
					b = ParNom(m.scene, "Bodofia");
					s = Sq(m, b);
					rendu = s != nullptr && Dist(Tete(*s, static_cast<uint32>(main), true), h) < 1e-4f;
				}
				Temoin(atteinte, "(e4) IK : la tete de la main tiree (vraie souris) arrive sur la cible", ecart);
				Temoin(sens, "(e4) le coude garde son sens pendant le geste", 0.f);
				Temoin(rendu, "(e4) Ctrl+Z rend la pose d'avant", 0.f);
				// CONTRE-EPREUVE : la meme IK ecrite SANS retenir -> Ctrl+Z ne la rend pas.
				NkEditeurOublierHistorique(m);
				s = Sq(m, b);
				bool revenu = true;
				if (s != nullptr && main >= 0) {
					const NkVec2f h = Tete(*s, static_cast<uint32>(main), true);
					uint32 mid = 0;
					NkVec2f eff;
					NkEditeurChaineIK(*s, static_cast<uint32>(main), false, mid, eff);
					unkeny::NkSqueletteIK2D(*s, mid, eff, h + NkVec2f(0.1f, 0.16f), true);
					NkEditeurAnnuler(m);
					b = ParNom(m.scene, "Bodofia");
					s = Sq(m, b);
					revenu = s != nullptr && Dist(Tete(*s, static_cast<uint32>(main), true), h) < 1e-4f;
					unkeny::NkSqueletteRetourRepos(*s);
				}
				Temoin(!revenu, "(e4n) sans retenir -> le temoin de Ctrl+Z voit ECHEC", 0.f);
				NkEditeurOublierHistorique(m);
			}

			// ── (e5) Poids : clic sur une tete = l'os ; un coup de pinceau ──
			{
				d = NkEditeurDocMaillageActif(ui);
				NkSquelette2D *s = Sq(m, b);
				const int32 cuisse = s != nullptr ? unkeny::NkSqueletteTrouverOs(*s, "CuisseD") : -1;
				bool choisi = false, peint = false, rendu = false;
				float32 apres = 0.f;
				if (d != nullptr && cuisse >= 0) {
					d->outil = NkOutilMaillage::NK_POIDS;
					d->os = -1;
					d->pinceauRayon = 0.12f;
					d->pinceauForce = 0.8f;
					T.Trame();
					const NkVec2f hc = NkEditeurMaillageVersEcran(*d, Tete(*s, static_cast<uint32>(cuisse), false));
					T.Clic(0, hc.x, hc.y);
					choisi = d->os == cuisse;
					// Le sommet le plus proche du torse : il a peu de « CuisseD ».
					const NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(b);
					const NkVec2f cible = Tete(*s, static_cast<uint32>(unkeny::NkSqueletteTrouverOs(*s, "Torse")), false);
					uint32 v = 0;
					for (uint32 i = 1; i < ml->nbSommets; ++i) {
						v = Dist(ml->positions[i], cible) < Dist(ml->positions[v], cible) ? i : v;
					}
					const float32 avant = unkeny::NkPoidsSommet2D(*ml, v, static_cast<uint32>(cuisse));
					const NkVec2f pv = NkEditeurMaillageVersEcran(*d, ml->positions[v]);
					T.Glisser(pv.x - 3.f, pv.y, pv.x + 3.f, pv.y, 3);
					ml = m.scene.Monde().Get<NkMaillage2D>(b);
					apres = unkeny::NkPoidsSommet2D(*ml, v, static_cast<uint32>(cuisse));
					peint = apres > avant + 0.3f;
					T.Touche(nkgui::NkGuiKey::Z, true);
					b = ParNom(m.scene, "Bodofia");
					ml = m.scene.Monde().Get<NkMaillage2D>(b);
					rendu = ml != nullptr && std::fabs(unkeny::NkPoidsSommet2D(*ml, v, static_cast<uint32>(cuisse)) - avant) < 1e-3f;
				}
				Temoin(choisi, "(e5) Poids : un clic sur la tete de CuisseD la choisit", 0.f);
				Temoin(peint, "(e5) un coup de pinceau lui donne du poids", apres);
				Temoin(rendu, "(e5) Ctrl+Z reprend le coup de pinceau", 0.f);
				NkEditeurOublierHistorique(m);
			}

			// ── (e6) La FRISE : les pistes d'os ──
			{
				m.selection = b;
				m.aSelection = true;
				NkEditeurOuvrirAnimation(m, ui, "");
				NkDocAnim *da = nullptr;
				for (uint32 k = 0; k < ui.pagesAnim.docs.Size(); ++k) {
					if (ui.pagesAnim.docs[k].genre == NkGenreDocAnim::NK_ANIMATION) {
						da = &ui.pagesAnim.docs[k];
					}
				}
				bool pistes = false, change = false, intact = false;
				uint32 nb = 0;
				if (da != nullptr) {
					da->cibleUid = m.scene.AssurerUid(b);
					NkEditeurProposerPistes(m, *da);
					for (uint32 k = 0; k < da->propositions.Size(); ++k) {
						if (da->propositions[k].propriete.nom == NkString("Os*")) {
							NkEditeurAjouterPisteProposee(m, *da, da->propositions[k]);
							break;
						}
					}
					for (uint32 k = 0; k < da->frise.tracks.Size(); ++k) {
						nb += da->frise.tracks[k].property == NkString(NK_FRISE_PROPRIETE_OS) && da->frise.tracks[k].keys.Size() == 1u ? 1u : 0u;
					}
					const NkSquelette2D *s = Sq(m, b);
					pistes = s != nullptr && nb == s->nbOs && da->clip.skeletalLocal;
					// Une cle d'os changee DANS la frise : l'angle du clip suit.
					for (uint32 k = 0; k < da->frise.tracks.Size(); ++k) {
						editorkit::NkTimelineTrack &t = da->frise.tracks[k];
						if (t.property == NkString(NK_FRISE_PROPRIETE_OS) && NkNomOsDePiste(t.object) == NkString("BrasD")) {
							t.keys[0].v[0] += 30.f;
						}
					}
					NkClipDepuisFrise(da->frise, da->clip);
					const int32 j = NkOsDuClipParNom(da->clip, "BrasD");
					const int32 jo = unkeny::NkSqueletteTrouverOs(*s, "BrasD");
					change = j >= 0 && jo >= 0 &&
							 std::fabs(anim::NkSampleBone2D(da->clip, static_cast<uint32>(j), 0.f).angle - (s->os[jo].pangle + 30.f * 3.14159265f / 180.f)) < 1e-3f;
					// L'apercu de la page : le squelette est rendu tel quel apres le dessin.
					const float32 avant = s->os[jo].pangle;
					NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, da->id));
					da->apercu = true;
					T.Trame();
					T.Trame();
					b = ParNom(m.scene, "Bodofia");
					intact = Sq(m, b) != nullptr && std::fabs(Sq(m, b)->os[jo].pangle - avant) < 1e-6f;
				}
				Temoin(pistes, "(e6) frise : « Tous les os » met une piste par os, une cle au curseur", static_cast<float32>(nb));
				Temoin(change, "(e6) une cle d'os changee dans la frise change le clip (+30 deg)", 0.f);
				Temoin(intact, "(e6) l'apercu de la frise ne laisse aucune trace dans la pose", 0.f);
				NkEditeurActiverDocument(m, ui, NkDocScene());
			}

			// ── (e7) JOUER : le clip d'os ; Arreter rend le repos ──
			{
				const NkSquelette2D *s = Sq(m, b);
				anim::NkAnimationClip marche = ClipMarche(*s);
				unkeny::NkEnregistrerClipProprietes("Marche", marche);
				unkeny::NkClipProprietes2D cp;
				cp.Jouer("Marche");
				m.scene.Monde().Add<unkeny::NkClipProprietes2D>(b, cp);
				const int32 cuisse = unkeny::NkSqueletteTrouverOs(*s, "CuisseD");
				const float32 repos = s->os[cuisse].angle;
				const NkMaillage2D *ml = m.scene.Monde().Get<NkMaillage2D>(b);
				const NkVec2f pied = Tete(*s, static_cast<uint32>(unkeny::NkSqueletteTrouverOs(*s, "PiedD")), false);
				uint32 v = 0;
				for (uint32 i = 1; i < ml->nbSommets; ++i) {
					v = Dist(ml->positions[i], pied) < Dist(ml->positions[v], pied) ? i : v;
				}
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(b);
				NkVec2f w0[NK_MAILLAGE2D_SOMMETS_MAX], w1[NK_MAILLAGE2D_SOMMETS_MAX];
				NkMaillagePositionsMonde(m.scene, b, *t, *ml, w0);
				NkEditeurJouer(m);
				for (int32 k = 0; k < 6; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				const float32 joue = s != nullptr ? s->os[cuisse].pangle - repos : 0.f;
				ml = m.scene.Monde().Get<NkMaillage2D>(b);
				t = m.scene.Monde().Get<NkTransform2D>(b);
				NkMaillagePositionsMonde(m.scene, b, *t, *ml, w1);
				const float32 bouge = Dist(w0[v], w1[v]);
				NkEditeurArreter(m);
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				Temoin(std::fabs(joue) > 0.2f && bouge > 0.02f, "(e7) Jouer : la marche plie la cuisse, la peau du pied bouge", bouge);
				Temoin(s != nullptr && std::fabs(s->os[cuisse].pangle - repos) < 1e-5f, "(e7) Arreter : la pose revient au repos", 0.f);
				m.scene.Monde().Remove<unkeny::NkClipProprietes2D>(b);
			}

			// ── (e8) La CHAINE MOLLE en Jouer ──
			{
				NkSquelette2D *s = Sq(m, b);
				const int32 cou = unkeny::NkSqueletteTrouverOs(*s, "Cou");
				const int32 e1 = NkEditeurSqueletteAjouterOs(m, b, cou, NkVec2f(0.f, 0.3f), NkVec2f(-0.25f, 0.3f), "Echarpe1");
				const int32 e2 = NkEditeurSqueletteAjouterOs(m, b, e1, NkVec2f(-0.25f, 0.3f), NkVec2f(-0.5f, 0.3f), "Echarpe2");
				NkEditeurSqueletteChaine(m, b, static_cast<uint32>(e1), 2);
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				const NkVec2f bout0 = Queue(*s, static_cast<uint32>(e2), true);
				NkEditeurJouer(m);
				for (int32 k = 0; k < 90; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				const float32 pend = s != nullptr ? bout0.y - Queue(*s, static_cast<uint32>(e2), true).y : 0.f;
				// Le corps FILE vers la droite : l'echarpe traine derriere (a gauche).
				float32 traine = 0.f;
				for (int32 k = 0; k < 24; ++k) {
					b = ParNom(m.scene, "Bodofia");
					if (NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(b)) {
						t->position.x += 0.06f;
					}
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				if (s != nullptr) {
					traine = Tete(*s, static_cast<uint32>(e1), true).x - Queue(*s, static_cast<uint32>(e2), true).x;
				}
				NkEditeurArreter(m);
				b = ParNom(m.scene, "Bodofia");
				s = Sq(m, b);
				const bool rendu = s != nullptr && Dist(Queue(*s, static_cast<uint32>(e2), true), bout0) < 1e-4f && s->chaines[0].corps == 0u;
				Temoin(pend > 0.2f, "(e8) Jouer : l'echarpe (chaine molle) PEND sous la gravite", pend);
				Temoin(traine > 0.1f, "(e8) le corps file a droite : l'echarpe TRAINE derriere", traine);
				Temoin(rendu, "(e8) Arreter : l'echarpe revient, plus de corps", 0.f);
			}

			// ── (e9) Enregistrer le squelette (.nkskel) ──
			{
				const NkString chemin = NkEditeurSqueletteEnregistrerAsset(m, b);
				anim::NkSkeletonDef def;
				bool deux = false;
				const bool lu = !chemin.Empty() && anim::NkLoadSkeletonDef(chemin, def, &deux);
				Temoin(lu && deux && def.Count() == Sq(m, b)->nbOs, "(e9) Enregistrer le squelette : un .nkskel que NKAnima relit (marque 2D)",
					   static_cast<float32>(def.Count()));
			}

			std::printf("== SQUELETTE EDITEUR : %d / %d ==\n", gR, gR + gE);
			al.Delete(pt);
			al.Delete(pm);
			return gE == 0 ? 0 : 1;
		}

		// =====================================================================
		// LES CAPTURES HORS ECRAN
		// =====================================================================
		int32 NkEditeurCapturesSquelette(const char *dossier) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkDirectory::CreateRecursive(dossier);
			const NkString fichiers = NkString::Format("%s/fichiers", dossier);
			NkDirectory::CreateRecursive(fichiers.CStr());
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_captures_squelette/";
			NkDirectory::Delete(racine.CStr(), true);
			NkDirectory::CreateRecursive((racine + "Contenu").CStr());
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			SceneNue(m);
			m.chemin = racine + "Bodofia.nkscene";
			NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			T.W = 1600.f;
			T.H = 900.f;
			T.pctx->Init(1600, 900);
			NkEditeurInterface &ui = T.Ui();
			ui.sonsMuets = true;
			ui.aimant = false;
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
			// Bodofia, debout sur le sol (y = -4 : ses pieds a -3,97).
			ecs::NkEntityId b0 = Bodofia(m, NkVec2f(0.f, -3.17f));
			NkEditeurCreerMaillage(m, b0, NkSourceMaillage::NK_SPRITE, 8);
			NkEditeurCreerSquelette(m, ParNom(m.scene, "Bodofia"), static_cast<int32>(anim::NkSkeleton2DTemplate::NK_HUMANOID_PROFILE));
			ecs::NkEntityId b = ParNom(m.scene, "Bodofia");
			SqueletteBodofia(m, b);
			NkEditeurOublierHistorique(m);
			m.selection = b;
			m.aSelection = true;
			m.scene.Camera().PoserCentre(NkVec2f(0.f, -3.1f));
			m.scene.Camera().PoserZoom(260.f);
			ui.cadrageEnAttente = false;
			ui.ongletDroite = 0;
			ui.detailsCategorie = 5; // la pastille Animation : la carte Squelette 2D
			ui.voirTiroir = false;
			Trames(4);
			Capture("01_personnage_et_ses_os.png");
			ui.voirTiroir = true;
			ui.detailsCategorie = 0;

			// ── La fenetre : les os, les poids ──
			NkEditeurOuvrirMaillage(m, ui, b);
			NkDocMaillage *d = NkEditeurDocMaillageActif(ui);
			if (d != nullptr) {
				d->outil = NkOutilMaillage::NK_OS;
				d->os = unkeny::NkSqueletteTrouverOs(*Sq(m, b), "BrasD");
				Trames(3);
				Capture("02_fenetre_os.png");
				d->outil = NkOutilMaillage::NK_POIDS;
				d->os = unkeny::NkSqueletteTrouverOs(*Sq(m, b), "CuisseD");
				Trames(2);
				Capture("03_poids_cuisse.png");
				// L'IK : la main de devant leve la braise (la VRAIE souris de la page).
				d->outil = NkOutilMaillage::NK_IK;
				d->coude = -1;
				Trames(2);
				const NkSquelette2D *s = Sq(m, b);
				const int32 main = unkeny::NkSqueletteTrouverOs(*s, "MainD");
				const NkVec2f h = Tete(*s, static_cast<uint32>(main), true);
				const NkVec2f a = NkEditeurMaillageVersEcran(*d, h), z = NkEditeurMaillageVersEcran(*d, h + NkVec2f(0.16f, 0.30f));
				T.pctx->input.mousePos = nkgui::NkVec2{a.x, a.y};
				T.Trame();
				T.pctx->input.mouseDown[0] = true;
				T.pctx->input.mouseClicked[0] = true;
				T.Trame();
				T.pctx->input.mouseClicked[0] = false;
				for (int32 k = 1; k <= 8; ++k) {
					const float32 u = static_cast<float32>(k) / 8.f;
					T.pctx->input.mousePos = nkgui::NkVec2{a.x + (z.x - a.x) * u, a.y + (z.y - a.y) * u};
					T.Trame();
				}
				// La capture PENDANT le geste : la cible se voit.
				erreurs += Png(T, NkString::Format("%s/%s", dossier, "04_pose_ik_main_levee.png").CStr()) ? 0 : 1;
				T.pctx->input.mouseDown[0] = false;
				T.Trame();
				NkEditeurSqueletteRepos(m, b);
				b = ParNom(m.scene, "Bodofia");
			}

			// ── La FRISE : la marche, ses pistes d'os ──
			const NkSquelette2D *s = Sq(m, b);
			anim::NkAnimationClip marche = ClipMarche(*s);
			unkeny::NkEnregistrerClipProprietes("Marche", marche);
			// Les MEMES fichiers pour NkAnimaEditor : le squelette et la marche.
			unkeny::NkSauverSquelette2D(*s, m.scene.Monde().Get<NkMaillage2D>(b), NkString::Format("%s/Bodofia.nkskel", fichiers.CStr()).CStr());
			marche.SaveBinary(NkString::Format("%s/Marche.nkanim", fichiers.CStr()));
			marche.SaveBinary(racine + "Contenu/Marche.nkanim");
			m.selection = b;
			NkEditeurOuvrirAnimation(m, ui, (racine + "Contenu/Marche.nkanim").CStr());
			for (uint32 k = 0; k < ui.pagesAnim.docs.Size(); ++k) {
				NkDocAnim &da = ui.pagesAnim.docs[k];
				if (da.genre == NkGenreDocAnim::NK_ANIMATION) {
					da.cibleUid = m.scene.AssurerUid(b);
					da.frise.cursor = 0.2f;
					da.apercu = true;
					NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, da.id));
				}
			}
			Trames(4);
			Capture("05_frise_pistes_os_marche.png");
			NkEditeurActiverDocument(m, ui, NkDocScene());

			// ── JOUER : la marche (deux instants), l'echarpe qui suit ──
			unkeny::NkClipProprietes2D cp;
			cp.Jouer("Marche");
			m.scene.Monde().Add<unkeny::NkClipProprietes2D>(b, cp);
			ui.voirTiroir = false;
			NkEditeurJouer(m);
			for (int32 k = 0; k < 60; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f); // l'echarpe se pose
			}
			// t = 1,0 s -> 0,2 dans la boucle ; puis + 0,4 s.
			m.scene.Camera().PoserCentre(NkVec2f(0.f, -3.1f));
			m.scene.Camera().PoserZoom(260.f);
			Trames(1);
			Capture("06_marche_instant_a.png");
			for (int32 k = 0; k < 24; ++k) {
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			Trames(1);
			Capture("07_marche_instant_b.png");
			// Bodofia file vers la droite : l'echarpe TRAINE derriere.
			for (int32 k = 0; k < 40; ++k) {
				ecs::NkEntityId bj = ParNom(m.scene, "Bodofia");
				if (NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(bj)) {
					t->position.x += 0.06f;
				}
				NkEditeurAvancer(m, 1.f / 60.f);
			}
			{
				ecs::NkEntityId bj = ParNom(m.scene, "Bodofia");
				if (const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(bj)) {
					m.scene.Camera().PoserCentre(NkVec2f(t->position.x, -3.1f));
				}
			}
			Trames(1);
			Capture("08_echarpe_molle_qui_suit.png");
			NkEditeurArreter(m);
			std::printf("== CAPTURES SQUELETTE : %d erreur(s) ==\n", erreurs);
			al.Delete(pt);
			al.Delete(pm);
			return erreurs;
		}

	} // namespace editeur
} // namespace nkentseu
