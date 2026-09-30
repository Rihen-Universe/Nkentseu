//
// NkUnkenyBancLumiere.cpp
// =============================================================================
// Description :
//   Le banc de l'eclairage 2D et des particules d'effet.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (L1)  ECLAIRAGE ETEINT = RIEN NE CHANGE : une scene (sprites, formes, et
//         meme une NkLumiere2D) dessinee avec les appels d'eclairage et
//         d'effets rend EXACTEMENT la liste d'affichage d'avant (sommets,
//         indices, commandes) et les memes pixels ; (L1n) allume, elle differe
//   (L2)  ambiante seule : un sprite blanc prend EXACTEMENT la teinte ambiante
//         (multiply), a 2 niveaux pres
//   (L3)  portee : a 1 m d'une ponctuelle lineaire de 2 m, ambiante + 0,5 ;
//         a 2 m et au-dela, l'AMBIANTE SEULE, exactement (pas un bit de plus) —
//         au calcul comme au pixel. [Amende apres le premier passage, qui a
//         echoue sur le TEMOIN et non sur la loi : voir le commentaire du test.]
//   (L4)  ombre : derriere une boite, l'ambiante seule ; devant, eclaire ; la
//         FACE de la boite reste eclairee ; un point hors du cone d'ombre
//         reste eclaire ; (L4n) ombres de la scene coupees : eclaire
//   (L5)  cone : dans le cone eclaire, a cote (meme distance) rien
//   (L6)  directionnelle : partout eclaire, sauf sous l'occulteur, et plus au-dela
//         de la longueur des ombres
//   (L7)  une lanterne DANS sa boite n'est pas masquee par elle
//   (L8)  repli VOILE : aucune commande MULTIPLY, pas d'ecran noir (le sprite
//         blanc a l'ambiante), et une lumiere l'eclaircit
//   (L9)  cout : une nuit sans lumiere tient en une bande par ligne de maille
//   (P1)  determinisme : meme graine = memes particules au bit pres (120 pas) ;
//         (P1n) autre graine, autres particules ; (P1c) avance a 30 i/s ou a
//         60 i/s : les memes (pas fixe interne)
//   (P2)  plafonds : celui de l'emetteur (50), celui de la scene (100) tiennent,
//         et les refus sont COMPTES
//   (P3)  les six presets emettent ; le feu monte et est additif, la pluie
//         tombe, la fumee est en alpha ; l'explosion lache sa rafale au premier
//         pas et s'eteint (particules et lumiere) apres sa vie
//   (P4)  couleur sur la vie : debut a 0, milieu a 0,5, fin a 1
//   (P5)  f6 : un emetteur ne change RIEN a la simulation (caisse au bit pres)
//   (P6)  le feu eclaire si l'eclairage est actif ; eclaire = faux : l'ambiante ;
//         l'eclairage eteint : (1, 1, 1) — rien n'est multiplie
//   (P7)  l'emetteur detruit : ses particules finissent leur vie, son horloge
//         est oubliee
//   (S1)  sauvegarde : lumiere, emetteur et reglages de scene reviennent champ
//         par champ ; (S1n) une entite sans lumiere n'en gagne pas
//   (S2)  un fichier d'AVANT se relit tel quel (eclairage eteint), et ce qu'on
//         en reecrit ne porte AUCUNE des nouvelles cles
//   (S3)  photo (Jouer / Arreter) : ambiante, lumiere et emetteur modifies en
//         jeu reviennent ; les particules repartent de zero
//   (S4)  un emetteur illisible (preset 99) : refuse, la scene est intacte
//   (S5)  [fusion avec physic2d-editeur-ue5, format v2] un fichier VERSION 1
//         portant lumiere, emetteur et eclairage (ce qu'ecrivait cette branche
//         avant la fusion) se relit, champ par champ
//   (S6)  un fichier VERSION 2 sans eclairage (ecrit par le moteur fusionne) se
//         relit eteint, avec ses entites, et ne gagne aucune des nouvelles cles
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBancLumiere.h"

#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKMemory/NKMemory.h"
#include "NKPlatform/NkEnv.h"
#include "Unkeny/Rendu/NkUnkenyEclairage.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyRenduEffets.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Simulation/NkUnkenyActeurs.h"

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

			/// Une scene SUR LE TAS : la pile du banc d'Unkeny est deja lourde
			/// (11 Mo en Debug, NkUnkenyBanc), on n'y ajoute pas.
			class NkSceneTas {
				public:
					NkSceneTas() {
						mScene = memory::NkGetDefaultAllocator().New<NkScene>();
						NkSceneConfig cfg;
						mScene->Init(cfg);
					}
					explicit NkSceneTas(const NkSceneConfig &cfg) {
						mScene = memory::NkGetDefaultAllocator().New<NkScene>();
						mScene->Init(cfg);
					}
					~NkSceneTas() {
						memory::NkGetDefaultAllocator().Delete(mScene);
					}
					NkSceneTas(const NkSceneTas &) = delete;
					NkSceneTas &operator=(const NkSceneTas &) = delete;
					NkScene &operator*() noexcept {
						return *mScene;
					}
					NkScene *operator->() noexcept {
						return mScene;
					}

				private:
					NkScene *mScene = nullptr;
			};

			/// La camera du banc : [-2, 2] x [-2, 2] dans 200 x 200 pixels, 50 px/m.
			/// Le monde (x, y) tombe au pixel (100 + 50 x, 100 - 50 y).
			void PoserCamera(NkScene &s) {
				s.Camera().PoserViseur(NkRect{0.f, 0.f, 200.f, 200.f});
				s.Camera().PoserCentre(NkVec2f(0.f, 0.f));
				s.Camera().PoserZoom(50.f);
			}

			/// Dessine comme le viseur de l'editeur : formes, sprites, effets
			/// eclaires, carte de lumiere, effets emissifs. `eclairage` faux :
			/// seulement ce que le moteur dessinait AVANT ce chantier.
			void Dessiner(nkgui::NkGuiDrawList &dl, NkScene &s, bool nouveaux) {
				NkDessinerFormes(dl, s);
				NkDessinerScene(dl, s);
				if (nouveaux) {
					NkDessinerEffets(dl, s, NkPasseEffets2D::NK_EFFETS_ECLAIRES);
					NkDessinerEclairage(dl, s);
					NkDessinerEffets(dl, s, NkPasseEffets2D::NK_EFFETS_EMISSIFS);
				}
			}

			struct NkImageBanc {
					nkgui::NkGuiContext ctx;
					nkgui::NkGuiDrawListRaster raster;
					void Rendre(NkScene &s, bool nouveaux) {
						PoserCamera(s);
						ctx.Init(200, 200);
						ctx.BeginFrame(1.f / 60.f);
						Dessiner(ctx.dl, s, nouveaux);
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

			/// Une image allouee sur le tas, pour la meme raison que la scene.
			class NkImageTas {
				public:
					NkImageTas() {
						mImage = memory::NkGetDefaultAllocator().New<NkImageBanc>();
					}
					~NkImageTas() {
						memory::NkGetDefaultAllocator().Delete(mImage);
					}
					NkImageTas(const NkImageTas &) = delete;
					NkImageTas &operator=(const NkImageTas &) = delete;
					NkImageBanc *operator->() noexcept {
						return mImage;
					}

				private:
					NkImageBanc *mImage = nullptr;
			};

			int32 Canal(uint32 px, int32 k) {
				return static_cast<int32>((px >> (24 - 8 * k)) & 0xFFu);
			}

			bool Proche(uint32 a, uint32 b, int32 tol) {
				for (int32 k = 0; k < 3; ++k) {
					const int32 d = Canal(a, k) - Canal(b, k);
					if (d > tol || -d > tol) {
						return false;
					}
				}
				return true;
			}

			bool MemesListes(const nkgui::NkGuiDrawList &a, const nkgui::NkGuiDrawList &b) {
				if (a.vtx.Size() != b.vtx.Size() || a.idx.Size() != b.idx.Size() || a.cmds.Size() != b.cmds.Size()) {
					return false;
				}
				for (uint32 i = 0; i < a.vtx.Size(); ++i) {
					const nkgui::NkGuiVertex &u = a.vtx[i];
					const nkgui::NkGuiVertex &v = b.vtx[i];
					if (u.pos.x != v.pos.x || u.pos.y != v.pos.y || u.uv.x != v.uv.x || u.uv.y != v.uv.y || u.col != v.col) {
						return false;
					}
				}
				for (uint32 i = 0; i < a.idx.Size(); ++i) {
					if (a.idx[i] != b.idx[i]) {
						return false;
					}
				}
				for (uint32 i = 0; i < a.cmds.Size(); ++i) {
					const nkgui::NkGuiDrawCmd &u = a.cmds[i];
					const nkgui::NkGuiDrawCmd &v = b.cmds[i];
					if (u.type != v.type || u.idxOffset != v.idxOffset || u.idxCount != v.idxCount || u.texId != v.texId ||
						u.blend != v.blend || u.clipRect.x != v.clipRect.x || u.clipRect.y != v.clipRect.y ||
						u.clipRect.w != v.clipRect.w || u.clipRect.h != v.clipRect.h) {
						return false;
					}
				}
				return true;
			}

			/// Un fond BLANC qui couvre toute la vue : l'image est alors la carte de
			/// lumiere elle-meme (blanc x lumiere = lumiere).
			void FondBlanc(NkScene &s) {
				const ecs::NkEntityId f = s.Creer("Fond", NkVec2f(0.f, 0.f));
				NkSprite2D sp;
				sp.taille = NkVec2f(5.f, 5.f);
				sp.couche = -100;
				s.Monde().Add<NkSprite2D>(f, sp);
			}

			ecs::NkEntityId Lumiere(NkScene &s, const NkVec2f &pos, const NkLumiere2D &l) {
				const ecs::NkEntityId e = s.Creer("Lumiere", pos);
				s.Monde().Add<NkLumiere2D>(e, l);
				return e;
			}

			ecs::NkEntityId Boite(NkScene &s, const NkVec2f &pos, const NkVec2f &demi) {
				const ecs::NkEntityId e = s.Creer("Boite", pos);
				NkCollisionneur2D c;
				c.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, c);
				return e;
			}

			float32 Lum(NkScene &s, float32 x, float32 y) {
				float32 rgb[3] = {0.f, 0.f, 0.f};
				NkLumiereAuPoint(s, NkVec2f(x, y), rgb);
				return rgb[0];
			}

			void Eclairer(NkScene &s, uint32 ambiante) {
				s.Eclairage().actif = true;
				s.Eclairage().ambiante = ambiante;
			}

			NkLumiere2D Blanche(float32 portee) {
				NkLumiere2D l;
				l.couleur = 0xFFFFFFFFu;
				l.intensite = 1.f;
				l.portee = portee;
				l.attenuation = 1.f;
				return l;
			}

			constexpr float32 AMB = 32.f / 255.f; ///< l'ambiante des temoins : 0x202020

			// =================================================================
			void BancEteint() {
				NkSceneTas s;
				FondBlanc(*s);
				const ecs::NkEntityId c = s->Creer("Caisse", NkVec2f(0.5f, 0.3f));
				NkSprite2D sp;
				sp.couleur = 0xC86432FFu;
				sp.taille = NkVec2f(0.8f, 0.6f);
				s->Monde().Add<NkSprite2D>(c, sp);
				Boite(*s, NkVec2f(-1.f, -1.f), NkVec2f(0.6f, 0.2f));
				Lumiere(*s, NkVec2f(0.f, 0.f), Blanche(2.f)); // une lumiere, mais la scene l'ignore
				PoserCamera(*s);

				nkgui::NkGuiDrawList avant;
				nkgui::NkGuiDrawList apres;
				Dessiner(avant, *s, false);
				Dessiner(apres, *s, true);
				NkImageTas ia;
				NkImageTas ib;
				ia->Rendre(*s, false);
				ib->Rendre(*s, true);
				const bool memesPx = std::memcmp(ia->raster.Pixels(), ib->raster.Pixels(), 200u * 200u * 4u) == 0;
				Temoin(MemesListes(avant, apres) && memesPx, "(L1) eteint : liste d'affichage ET pixels identiques a avant",
					   static_cast<float32>(apres.vtx.Size()));

				s->Eclairage().actif = true;
				nkgui::NkGuiDrawList allume;
				Dessiner(allume, *s, true);
				ib->Rendre(*s, true);
				const bool autresPx = std::memcmp(ia->raster.Pixels(), ib->raster.Pixels(), 200u * 200u * 4u) != 0;
				Temoin(!MemesListes(avant, allume) && autresPx, "(L1n) allume : la liste et l'image changent",
					   static_cast<float32>(allume.vtx.Size()));
			}

			void BancAmbianteEtPortee() {
				NkSceneTas s;
				FondBlanc(*s);
				Eclairer(*s, 0x406080FFu);
				NkImageTas im;
				im->Rendre(*s, true);
				Temoin(Proche(im->Px(0.3f, 0.7f), 0x406080FFu, 2), "(L2) ambiante seule : blanc x ambiante = ambiante",
					   static_cast<float32>(Canal(im->Px(0.3f, 0.7f), 0)));

				s->Eclairage().ambiante = 0x202020FFu;
				Lumiere(*s, NkVec2f(0.f, 0.f), Blanche(2.f));
				const float32 a1 = Lum(*s, 1.f, 0.f);
				const float32 bord = Lum(*s, 1.999f, 0.f);
				// ⚠️ LE BORD LUI-MEME SE TESTE SUR L'AXE. Le premier passage a echoue la
				//    en partant de r = 2 m : (2 cos a, 2 sin a) retombe en float32 un
				//    rien EN DECA de 2 m pour certains angles, et y recoit une lumiere
				//    infime -- juste pour la loi, faux pour le temoin (la ligne
				//    [MESURE] ci-dessous le chiffre). Sur l'axe, (2, 0) est a 2 m pile ;
				//    hors de l'axe, on part de 2,001 m.
				const bool bordPile = Lum(*s, 2.f, 0.f) == AMB && Lum(*s, 0.f, -2.f) == AMB;
				{
					// La MESURE qui justifie l'amendement (imprimee, pas comptee).
					int32 nbBord = 0;
					float32 exces = 0.f;
					for (int32 k = 0; k < 64; ++k) {
						const float32 ang = 6.2831853f * static_cast<float32>(k) / 64.f;
						const float32 v = Lum(*s, 2.f * math::NkCos(ang), 2.f * math::NkSin(ang)) - AMB;
						nbBord += v != 0.f ? 1 : 0;
						exces = v > exces ? v : exces;
					}
					std::printf("  [MESURE] a r = 2 m (2 cos a, 2 sin a) : %d directions sur 64 recoivent %.3g au plus\n", nbBord,
								static_cast<double>(exces));
				}
				bool auDela = true;
				float32 premierEchec = 0.f;
				for (int32 k = 0; k < 64; ++k) {
					const float32 ang = 6.2831853f * static_cast<float32>(k) / 64.f;
					for (float32 r = 2.001f; r < 3.f; r += 0.125f) {
						const bool ok = Lum(*s, r * math::NkCos(ang), r * math::NkSin(ang)) == AMB;
						if (!ok && auDela) {
							premierEchec = r;
						}
						auDela = auDela && ok;
					}
				}
				Temoin(math::NkAbs(a1 - (AMB + 0.5f)) < 1.0e-3f && bord > AMB, "(L3) a 1 m d'une portee de 2 m : ambiante + 0,5",
					   a1);
				Temoin(bordPile && auDela, "(L3) a 2 m pile, puis de 2,001 a 3 m sur 64 directions : l'ambiante SEULE",
					   premierEchec);
				im->Rendre(*s, true);
				const uint32 dedans = im->Px(1.f, 0.f);
				const uint32 dehors = im->Px(1.8f, 1.8f);
				Temoin(Canal(dedans, 0) > 150 && Canal(dedans, 0) < 170 && Proche(dehors, 0x202020FFu, 1),
					   "(L3) au pixel : 1 m eclaire (~159), 2,5 m a l'ambiante (32)", static_cast<float32>(Canal(dehors, 0)));
			}

			void BancOmbres() {
				NkSceneTas s;
				FondBlanc(*s);
				Eclairer(*s, 0x202020FFu);
				Lumiere(*s, NkVec2f(-1.5f, 0.f), Blanche(4.f));
				Boite(*s, NkVec2f(0.f, 0.f), NkVec2f(0.2f, 0.6f));
				const float32 derriere = Lum(*s, 1.2f, 0.f);
				const float32 devant = Lum(*s, -0.8f, 0.f);
				const float32 face = Lum(*s, -0.15f, 0.f);
				const float32 aCote = Lum(*s, 1.2f, 1.5f);
				Temoin(derriere == AMB && devant > AMB + 0.5f, "(L4) derriere la boite : l'ambiante ; devant : eclaire", derriere);
				Temoin(face > AMB + 0.3f && aCote > AMB + 0.1f, "(L4) la FACE de la boite, et hors du cone d'ombre : eclaires",
					   face);
				NkImageTas im;
				im->Rendre(*s, true);
				const uint32 pxOmbre = im->Px(1.2f, 0.f);
				s->Eclairage().ombres = false;
				const float32 sansOmbre = Lum(*s, 1.2f, 0.f);
				im->Rendre(*s, true);
				const uint32 pxSans = im->Px(1.2f, 0.f);
				Temoin(Proche(pxOmbre, 0x202020FFu, 2) && Canal(pxSans, 0) > 60 && sansOmbre > AMB + 0.2f,
					   "(L4n) au pixel : ombre a l'ambiante ; ombres coupees, eclaire", static_cast<float32>(Canal(pxSans, 0)));
			}

			void BancConeDirectionnelleLanterne() {
				{
					NkSceneTas s;
					Eclairer(*s, 0x202020FFu);
					NkLumiere2D l = Blanche(3.f);
					l.type = NkTypeLumiere2D::NK_SPOT;
					l.direction = -1.5707963f; // vers le bas
					l.ouverture = 0.5f;
					l.douceur = 0.f;
					Lumiere(*s, NkVec2f(0.f, 0.f), l);
					const float32 dans = Lum(*s, 0.f, -1.f);
					const float32 cote = Lum(*s, 1.f, 0.f);
					const float32 dos = Lum(*s, 0.f, 1.f);
					Temoin(dans > AMB + 0.5f && cote == AMB && dos == AMB, "(L5) cone : dans le cone eclaire, a cote et au dos rien",
						   dans);
				}
				{
					NkSceneTas s;
					Eclairer(*s, 0x202020FFu);
					NkLumiere2D l = Blanche(3.f); // 3 m : la longueur des ombres
					l.type = NkTypeLumiere2D::NK_DIRECTIONNELLE;
					l.intensite = 0.5f;
					l.direction = -1.5707963f; // la lumiere descend
					Lumiere(*s, NkVec2f(5.f, 5.f), l);
					Boite(*s, NkVec2f(0.f, 1.f), NkVec2f(0.5f, 0.2f));
					const float32 dessous = Lum(*s, 0.f, 0.f);
					const float32 ailleurs = Lum(*s, 2.f, 0.f);
					const float32 loin = Lum(*s, 0.f, -3.f);
					const float32 attendu = AMB + 0.5f;
					Temoin(dessous == AMB && math::NkAbs(ailleurs - attendu) < 1.0e-5f && math::NkAbs(loin - attendu) < 1.0e-5f,
						   "(L6) directionnelle : ombre sous la boite, pas au-dela de 3 m", ailleurs);
				}
				{
					NkSceneTas s;
					Eclairer(*s, 0x202020FFu);
					const ecs::NkEntityId lan = Lumiere(*s, NkVec2f(0.f, 0.f), Blanche(3.f));
					NkCollisionneur2D c;
					c.demiTaille = NkVec2f(0.3f, 0.3f);
					s->Monde().Add<NkCollisionneur2D>(lan, c);
					const float32 v = Lum(*s, 1.f, 0.f);
					Temoin(v > AMB + 0.5f, "(L7) une lanterne DANS sa boite eclaire au dehors", v);
				}
			}

			void BancVoileEtCout() {
				NkSceneTas s;
				FondBlanc(*s);
				Eclairer(*s, 0x4C4C4CFFu);
				nkgui::NkGuiDrawList nuit;
				PoserCamera(*s);
				const NkStatsEclairage2D st = NkDessinerEclairage(nuit, *s);
				Temoin(st.mailles <= 26 && st.points == 26 * 26, "(L9) nuit sans lumiere : une bande par ligne de maille (quads)",
					   static_cast<float32>(st.mailles));

				s->Eclairage().mode = NkModeEclairage2D::NK_VOILE;
				nkgui::NkGuiDrawList voile;
				NkDessinerEclairage(voile, *s);
				bool multiply = false;
				for (uint32 i = 0; i < voile.cmds.Size(); ++i) {
					multiply = multiply || voile.cmds[i].blend == nkgui::NkGuiBlend::Multiply;
				}
				NkImageTas im;
				im->Rendre(*s, true);
				const uint32 sombre = im->Px(0.5f, 0.5f);
				NkLumiere2D l = Blanche(2.f);
				Lumiere(*s, NkVec2f(0.5f, 0.5f), l);
				im->Rendre(*s, true);
				const uint32 clair = im->Px(0.8f, 0.5f);
				Temoin(!multiply && Proche(sombre, 0x4C4C4CFFu, 3) && Canal(clair, 0) > 150,
					   "(L8) voile : sans MULTIPLY, l'ambiante, et la lumiere eclaircit", static_cast<float32>(Canal(sombre, 0)));
			}

			// =================================================================
			// Les particules
			// =================================================================
			ecs::NkEntityId Emetteur(NkScene &s, const NkVec2f &pos, const NkEmetteur2D &e) {
				const ecs::NkEntityId id = s.Creer("Emetteur", pos);
				s.Monde().Add<NkEmetteur2D>(id, e);
				return id;
			}

			bool MemesParticules(const NkEffets2D &a, const NkEffets2D &b) {
				if (a.NbParticules() != b.NbParticules()) {
					return false;
				}
				for (uint32 i = 0; i < a.NbParticules(); ++i) {
					const NkParticuleEffet2D &p = a.Particules()[i];
					const NkParticuleEffet2D &q = b.Particules()[i];
					if (p.pos.x != q.pos.x || p.pos.y != q.pos.y || p.vit.x != q.vit.x || p.vit.y != q.vit.y ||
						p.age != q.age || p.vie != q.vie || p.taille0 != q.taille0) {
						return false;
					}
				}
				return true;
			}

			void BancDeterminisme() {
				NkEmetteur2D feu = NkPresetEmetteur2D(NkPresetEffet2D::NK_FEU);
				feu.graine = 7u;
				NkSceneTas a;
				NkSceneTas b;
				NkSceneTas c;
				NkSceneTas d;
				Emetteur(*a, NkVec2f(0.f, 0.f), feu);
				Emetteur(*b, NkVec2f(0.f, 0.f), feu);
				NkEmetteur2D autre = feu;
				autre.graine = 8u;
				Emetteur(*c, NkVec2f(0.f, 0.f), autre);
				Emetteur(*d, NkVec2f(0.f, 0.f), feu);
				for (int32 k = 0; k < 120; ++k) {
					a->Effets().Pas(*a);
					b->Effets().Pas(*b);
					c->Effets().Pas(*c);
				}
				// d : la meme duree (2 s), mais en trames de 1/30 s par Avancer.
				for (int32 k = 0; k < 60; ++k) {
					d->Effets().Avancer(*d, 1.f / 30.f);
				}
				Temoin(a->Effets().NbParticules() > 20u && MemesParticules(a->Effets(), b->Effets()),
					   "(P1) meme graine, 120 pas : memes particules au bit pres", static_cast<float32>(a->Effets().NbParticules()));
				Temoin(!MemesParticules(a->Effets(), c->Effets()), "(P1n) autre graine : autres particules", 0.f);
				Temoin(MemesParticules(a->Effets(), d->Effets()), "(P1c) 60 trames de 1/30 s = 120 pas de 1/60 s",
					   static_cast<float32>(d->Effets().NbParticules()));
			}

			void BancPlafonds() {
				{
					NkSceneTas s;
					NkEmetteur2D e;
					e.debit = 100000.f;
					e.vieMin = 5.f;
					e.vieMax = 5.f;
					e.maxParticules = 50u;
					Emetteur(*s, NkVec2f(0.f, 0.f), e);
					for (int32 k = 0; k < 10; ++k) {
						s->Effets().Pas(*s);
					}
					Temoin(s->Effets().NbParticules() == 50u && s->Effets().Refusees() > 0u,
						   "(P2) plafond de l'emetteur (50) tenu, refus comptes", static_cast<float32>(s->Effets().Refusees()));
				}
				{
					NkSceneTas s;
					s->Effets().plafond = 100u;
					NkEmetteur2D e;
					e.debit = 10000.f;
					e.vieMin = 5.f;
					e.vieMax = 5.f;
					e.maxParticules = 80u;
					for (int32 k = 0; k < 3; ++k) {
						Emetteur(*s, NkVec2f(static_cast<float32>(k), 0.f), e);
					}
					for (int32 k = 0; k < 10; ++k) {
						s->Effets().Pas(*s);
					}
					Temoin(s->Effets().NbParticules() == 100u, "(P2) plafond de la scene (100) tenu, trois emetteurs de 80",
						   static_cast<float32>(s->Effets().NbParticules()));
				}
			}

			void BancPresets() {
				bool tous = true;
				float32 vyFeu = 0.f;
				float32 vyPluie = 0.f;
				bool feuAdditif = false;
				bool fumeeAlpha = false;
				for (int32 p = 1; p < static_cast<int32>(NkPresetEffet2D::NK_COUNT); ++p) {
					NkSceneTas s;
					const NkPresetEffet2D preset = static_cast<NkPresetEffet2D>(p);
					Emetteur(*s, NkVec2f(0.f, 0.f), NkPresetEmetteur2D(preset));
					for (int32 k = 0; k < 30; ++k) {
						s->Effets().Pas(*s);
					}
					const NkEffets2D &fx = s->Effets();
					tous = tous && fx.NbParticules() > 0u;
					float32 vy = 0.f;
					for (uint32 i = 0; i < fx.NbParticules(); ++i) {
						vy += fx.Particules()[i].vit.y;
					}
					vy = fx.NbParticules() > 0u ? vy / static_cast<float32>(fx.NbParticules()) : 0.f;
					if (preset == NkPresetEffet2D::NK_FEU) {
						vyFeu = vy;
						feuAdditif = fx.NbParticules() > 0u && fx.Particules()[0].additif;
					}
					if (preset == NkPresetEffet2D::NK_PLUIE) {
						vyPluie = vy;
					}
					if (preset == NkPresetEffet2D::NK_FUMEE) {
						fumeeAlpha = fx.NbParticules() > 0u && !fx.Particules()[0].additif;
					}
				}
				Temoin(tous && vyFeu > 0.f && feuAdditif && vyPluie < 0.f && fumeeAlpha,
					   "(P3) six presets emettent ; feu monte (additif), pluie tombe, fumee en alpha", vyFeu);

				NkSceneTas s;
				const NkEmetteur2D ex = NkPresetEmetteur2D(NkPresetEffet2D::NK_EXPLOSION);
				const ecs::NkEntityId id = Emetteur(*s, NkVec2f(0.f, 0.f), ex);
				s->Effets().Pas(*s);
				const uint32 rafale = s->Effets().NbParticules();
				const float32 lumiereTot = s->Effets().FacteurLumiere(id.Pack(), ex);
				for (int32 k = 0; k < 180; ++k) {
					s->Effets().Pas(*s);
				}
				const float32 lumiereTard = s->Effets().FacteurLumiere(id.Pack(), ex);
				Temoin(rafale == ex.rafale && lumiereTot == 1.f && s->Effets().NbParticules() == 0u && lumiereTard == 0.f,
					   "(P3) explosion : sa rafale au 1er pas, puis plus rien (lumiere comprise)", static_cast<float32>(rafale));
			}

			void BancCouleurEtSimulation() {
				const uint32 a = 0xFF000080u;
				const uint32 b = 0x00FF0040u;
				const uint32 c = 0x0000FF00u;
				Temoin(NkCouleurSurLaVie(a, b, c, 0.f) == a && NkCouleurSurLaVie(a, b, c, 0.5f) == b &&
						   NkCouleurSurLaVie(a, b, c, 1.f) == c,
					   "(P4) couleur sur la vie : debut, milieu a 0,5, fin", 0.f);

				NkSceneConfig cfg;
				cfg.physique = true;
				auto Caisse = [](NkScene &s) {
					const ecs::NkEntityId sol = s.Creer("Sol", NkVec2f(0.f, -1.f));
					NkCollisionneur2D cs;
					cs.demiTaille = NkVec2f(5.f, 0.5f);
					s.Monde().Add<NkCollisionneur2D>(sol, cs);
					NkCorps2D ks;
					ks.type = NkTypeCorps::NK_STATIQUE;
					s.AjouterCorps(sol, ks);
					const ecs::NkEntityId k = s.Creer("Caisse", NkVec2f(0.f, 2.f));
					NkCollisionneur2D ck;
					ck.demiTaille = NkVec2f(0.3f, 0.3f);
					s.Monde().Add<NkCollisionneur2D>(k, ck);
					NkCorps2D kk;
					s.AjouterCorps(k, kk);
					return k;
				};
				NkSceneTas sa(cfg);
				NkSceneTas sb(cfg);
				const ecs::NkEntityId ka = Caisse(*sa);
				const ecs::NkEntityId kb = Caisse(*sb);
				Emetteur(*sb, NkVec2f(0.f, 0.f), NkPresetEmetteur2D(NkPresetEffet2D::NK_ETINCELLES));
				for (int32 k = 0; k < 90; ++k) {
					sa->Pas(1.f / 60.f);
					sb->Pas(1.f / 60.f);
				}
				const NkTransform2D *ta = sa->Monde().Get<NkTransform2D>(ka);
				const NkTransform2D *tb = sb->Monde().Get<NkTransform2D>(kb);
				Temoin(ta != nullptr && tb != nullptr && ta->position.x == tb->position.x && ta->position.y == tb->position.y &&
						   sb->Effets().NbParticules() > 0u,
					   "(P5) f6 : un emetteur ne change rien a la simulation (au bit pres)", tb != nullptr ? tb->position.y : 0.f);
			}

			void BancFeuQuiEclaire() {
				NkSceneTas s;
				Eclairer(*s, 0x202020FFu);
				NkEmetteur2D feu = NkPresetEmetteur2D(NkPresetEffet2D::NK_FEU);
				feu.scintillement = 0.f;
				const ecs::NkEntityId id = Emetteur(*s, NkVec2f(0.f, 0.f), feu);
				const float32 pres = Lum(*s, 1.f, 0.f);
				s->Monde().Get<NkEmetteur2D>(id)->eclaire = false;
				const float32 eteint = Lum(*s, 1.f, 0.f);
				s->Monde().Get<NkEmetteur2D>(id)->eclaire = true;
				s->Eclairage().actif = false;
				const float32 sansEclairage = Lum(*s, 1.f, 0.f);
				s->Eclairage().actif = true;
				// Le vacillement : borne par [1 - scintillement, 1].
				s->Monde().Get<NkEmetteur2D>(id)->scintillement = 0.3f;
				float32 mini = 2.f;
				float32 maxi = -1.f;
				for (int32 k = 0; k < 120; ++k) {
					s->Effets().Pas(*s);
					const float32 f = s->Effets().FacteurLumiere(id.Pack(), *s->Monde().Get<NkEmetteur2D>(id));
					mini = f < mini ? f : mini;
					maxi = f > maxi ? f : maxi;
				}
				Temoin(pres > AMB + 0.3f && eteint == AMB && sansEclairage == 1.f,
					   "(P6) le feu eclaire ; eclaire = faux : l'ambiante ; eteint : 1", pres);
				Temoin(mini >= 0.7f - 1.0e-5f && maxi <= 1.f && maxi - mini > 0.01f, "(P6) il vacille, entre 1 - 0,3 et 1", mini);
			}

			void BancDestruction() {
				NkSceneTas s;
				const ecs::NkEntityId id = Emetteur(*s, NkVec2f(0.f, 0.f), NkPresetEmetteur2D(NkPresetEffet2D::NK_FEU));
				for (int32 k = 0; k < 30; ++k) {
					s->Effets().Pas(*s);
				}
				s->Detruire(id);
				s->Effets().Pas(*s);
				const uint32 juste = s->Effets().NbParticules();
				for (int32 k = 0; k < 60; ++k) {
					s->Effets().Pas(*s);
				}
				Temoin(juste > 0u && s->Effets().NbParticules() == 0u && s->Effets().Etat(id.Pack()) == nullptr,
					   "(P7) emetteur detruit : ses particules finissent, son horloge est oubliee", static_cast<float32>(juste));
			}

			// =================================================================
			// La sauvegarde et la photo
			// =================================================================
			bool MemeLumiere(const NkLumiere2D &a, const NkLumiere2D &b) {
				return a.type == b.type && a.couleur == b.couleur && a.intensite == b.intensite && a.portee == b.portee &&
					   a.attenuation == b.attenuation && a.direction == b.direction && a.ouverture == b.ouverture &&
					   a.douceur == b.douceur && a.decalage.x == b.decalage.x && a.decalage.y == b.decalage.y &&
					   a.halo == b.halo && a.ombres == b.ombres && a.actif == b.actif;
			}

			bool MemeEmetteur(const NkEmetteur2D &a, const NkEmetteur2D &b) {
				return a.preset == b.preset && a.actif == b.actif && a.boucle == b.boucle && a.duree == b.duree &&
					   a.rafale == b.rafale && a.debit == b.debit && a.vieMin == b.vieMin && a.vieMax == b.vieMax &&
					   a.vitesseMin == b.vitesseMin && a.vitesseMax == b.vitesseMax && a.direction == b.direction &&
					   a.dispersion == b.dispersion && a.gravite.x == b.gravite.x && a.gravite.y == b.gravite.y &&
					   a.frein == b.frein && a.tailleDebut == b.tailleDebut && a.tailleFin == b.tailleFin &&
					   a.couleurDebut == b.couleurDebut && a.couleurMilieu == b.couleurMilieu && a.couleurFin == b.couleurFin &&
					   a.additif == b.additif && a.forme == b.forme && a.zone == b.zone && a.rayonZone == b.rayonZone &&
					   a.largeurZone == b.largeurZone && a.decalage.x == b.decalage.x && a.decalage.y == b.decalage.y &&
					   a.graine == b.graine && a.maxParticules == b.maxParticules && a.eclaire == b.eclaire &&
					   a.couleurLumiere == b.couleurLumiere && a.intensiteLumiere == b.intensiteLumiere &&
					   a.porteeLumiere == b.porteeLumiere && a.scintillement == b.scintillement &&
					   a.ombresLumiere == b.ombresLumiere;
			}

			bool MemeEclairage(const NkEclairage2D &a, const NkEclairage2D &b) {
				return a.actif == b.actif && a.ambiante == b.ambiante && a.ombres == b.ombres &&
					   a.masqueOcculteurs == b.masqueOcculteurs && a.mode == b.mode && a.maille == b.maille;
			}

			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId trouve;
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strcmp(e.nom, nom) == 0) {
						trouve = id;
					}
				});
				return trouve;
			}

			bool SansNouvellesCles(const NkString &json) {
				const char *t = json.CStr();
				return std::strstr(t, "\"lumiere\"") == nullptr && std::strstr(t, "\"emetteur\"") == nullptr &&
					   std::strstr(t, "\"eclairage\"") == nullptr;
			}

			void BancSauvegarde() {
				NkSceneTas a;
				a->Eclairage().actif = true;
				a->Eclairage().ambiante = 0x10203040u;
				a->Eclairage().ombres = false;
				a->Eclairage().masqueOcculteurs = 0x3u;
				a->Eclairage().mode = NkModeEclairage2D::NK_VOILE;
				a->Eclairage().maille = 16.f;
				NkLumiere2D l;
				l.type = NkTypeLumiere2D::NK_SPOT;
				l.couleur = 0x11223344u;
				l.intensite = 1.75f;
				l.portee = 6.5f;
				l.attenuation = 1.5f;
				l.direction = 0.125f;
				l.ouverture = 0.3f;
				l.douceur = 0.6f;
				l.decalage = NkVec2f(0.1f, -0.2f);
				l.halo = 0.4f;
				l.ombres = false;
				l.actif = false;
				const ecs::NkEntityId el = a->Creer("Phare", NkVec2f(1.f, 2.f));
				a->Monde().Add<NkLumiere2D>(el, l);
				NkEmetteur2D e = NkPresetEmetteur2D(NkPresetEffet2D::NK_FUMEE);
				e.graine = 12345u;
				e.rafale = 7u;
				e.boucle = false;
				e.duree = 3.25f;
				e.forme = NkFormeParticule2D::NK_PLEINE;
				e.zone = NkZoneEmission2D::NK_LIGNE;
				e.largeurZone = 2.5f;
				e.eclaire = true;
				e.scintillement = 0.2f;
				e.ombresLumiere = false;
				e.decalage = NkVec2f(0.3f, 0.4f);
				const ecs::NkEntityId ee = a->Creer("Cheminee", NkVec2f(-1.f, 0.f));
				a->Monde().Add<NkEmetteur2D>(ee, e);
				a->Creer("Rien", NkVec2f(0.f, 0.f));

				NkString json;
				const bool ecrit = NkSauverSceneJSON(*a, json);
				NkSceneTas b;
				NkString err;
				const bool lu = NkChargerSceneJSON(*b, json.View(), nullptr, &err);
				const ecs::NkEntityId bl = ParNom(*b, "Phare");
				const ecs::NkEntityId be = ParNom(*b, "Cheminee");
				const ecs::NkEntityId br = ParNom(*b, "Rien");
				const NkLumiere2D *lb = bl.IsValid() ? b->Monde().Get<NkLumiere2D>(bl) : nullptr;
				const NkEmetteur2D *eb = be.IsValid() ? b->Monde().Get<NkEmetteur2D>(be) : nullptr;
				Temoin(ecrit && lu && lb != nullptr && eb != nullptr && MemeLumiere(l, *lb) && MemeEmetteur(e, *eb) &&
						   MemeEclairage(a->Eclairage(), b->Eclairage()),
					   "(S1) lumiere, emetteur et eclairage : relus champ par champ", static_cast<float32>(json.Length()) / 1024.f);
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}
				Temoin(br.IsValid() && !b->Monde().Has<NkLumiere2D>(br) && !b->Monde().Has<NkEmetteur2D>(br),
					   "(S1n) une entite sans lumiere n'en gagne pas a la relecture", 0.f);

				// (S2) un fichier d'AVANT, tel que l'ecrivait le moteur du 29/09.
				static const char *kAncien =
					"{\"format\":\"unkeny.scene\",\"version\":1,"
					"\"config\":{\"physique\":false,\"particules\":false,\"gravite\":\"0 -9.81000042\","
					"\"pasFixe\":0.0166666675,\"pasMaxParTrame\":5},\"camera\":\"0 0 32 0\","
					"\"entites\":[{\"nom\":\"Caisse\",\"transform\":\"1 2 0 1 1\",\"sprite\":{\"taille\":\"1 1\","
					"\"pivot\":\"0.5 0.5\",\"couleur\":255,\"uv\":\"0 0 1 1\",\"couche\":0,\"visible\":true}}]}";
				NkSceneTas c;
				const bool luAncien = NkChargerSceneJSON(*c, NkStringView(kAncien), nullptr, &err);
				NkString reecrit;
				NkSauverSceneJSON(*c, reecrit);
				const ecs::NkEntityId caisse = ParNom(*c, "Caisse");
				const NkSprite2D *sp = caisse.IsValid() ? c->Monde().Get<NkSprite2D>(caisse) : nullptr;
				Temoin(luAncien && sp != nullptr && sp->couleur == 255u && !c->Eclairage().actif && SansNouvellesCles(reecrit),
					   "(S2) fichier d'avant : relu, eteint, reecrit SANS nouvelle cle", static_cast<float32>(reecrit.Length()));

				// (S4) un emetteur illisible : refuse, la scene cible est intacte.
				NkString mauvais(json);
				const char *cle = "\"preset\":";
				const char *ou = std::strstr(mauvais.CStr(), cle);
				bool refuse = false;
				if (ou != nullptr) {
					const usize at = static_cast<usize>(ou - mauvais.CStr()) + std::strlen(cle);
					NkString avant(mauvais.CStr(), at);
					const char *apres = mauvais.CStr() + at;
					while (*apres == ' ' || (*apres >= '0' && *apres <= '9')) {
						++apres;
					}
					NkString refait = avant;
					refait.Append("99");
					refait.Append(apres);
					NkVector<ecs::NkEntityId> avantIds;
					c->Entites(avantIds);
					refuse = !NkChargerSceneJSON(*c, refait.View(), nullptr, &err);
					NkVector<ecs::NkEntityId> apresIds;
					c->Entites(apresIds);
					refuse = refuse && apresIds.Size() == avantIds.Size() && ParNom(*c, "Caisse").IsValid();
				}
				Temoin(refuse, "(S4) emetteur illisible (preset 99) : refuse, scene intacte", 0.f);

				// (S5) un fichier v1 AVEC eclairage : ce que la branche ecrivait avant
				//      de rencontrer le format v2 (memes cles, memes ecritures).
				static const char *kV1Lumiere =
					"{\"format\":\"unkeny.scene\",\"version\":1,"
					"\"config\":{\"physique\":false,\"particules\":false,\"gravite\":\"0 -9.81000042\","
					"\"pasFixe\":0.016666667535901070,\"pasMaxParTrame\":5},\"camera\":\"0 0 32 0\","
					"\"entites\":[{\"nom\":\"Lampe\",\"transform\":\"1 2 0 1 1\",\"lumiere\":{\"type\":1,"
					"\"couleur\":287454020,\"intensite\":1.75,\"portee\":6.5,\"attenuation\":1.5,"
					"\"direction\":0.125,\"ouverture\":0.30000001192092896,\"douceur\":0.60000002384185791,"
					"\"decalage\":\"0.100000001 -0.200000003\",\"halo\":0.40000000596046448,\"ombres\":false,"
					"\"actif\":false}},{\"nom\":\"Feu\",\"transform\":\"0 0 0 1 1\",\"emetteur\":{\"preset\":1,"
					"\"actif\":true,\"boucle\":true,\"duree\":1,\"rafale\":0,\"debit\":70,\"vie\":\"0.449999988 0.899999976\","
					"\"vitesse\":\"0.5 1.20000005\",\"direction\":1.5707963705062866,\"dispersion\":0.31999999284744263,"
					"\"gravite\":\"0 1.60000002\",\"frein\":1.2000000476837158,\"taille\":\"0.419999987 0.0799999982\","
					"\"couleurs\":\"4293959935 4286193352 1846804480\",\"additif\":true,\"forme\":0,\"zone\":1,"
					"\"rayonZone\":0.18000000715255737,\"largeurZone\":0,\"decalage\":\"0 0\",\"graine\":4242,"
					"\"max\":160,\"eclaire\":true,\"couleurLumiere\":4288301311,\"intensiteLumiere\":1.25,"
					"\"porteeLumiere\":5,\"scintillement\":0.30000001192092896,\"ombresLumiere\":true}}],"
					"\"eclairage\":{\"actif\":true,\"ambiante\":270544960,\"ombres\":false,\"masqueOcculteurs\":3,"
					"\"mode\":1,\"maille\":16}}";
				NkSceneTas v1;
				const bool luV1 = NkChargerSceneJSON(*v1, NkStringView(kV1Lumiere), nullptr, &err);
				const ecs::NkEntityId lampe = ParNom(*v1, "Lampe");
				const ecs::NkEntityId feuV1 = ParNom(*v1, "Feu");
				const NkLumiere2D *l1 = lampe.IsValid() ? v1->Monde().Get<NkLumiere2D>(lampe) : nullptr;
				const NkEmetteur2D *e1 = feuV1.IsValid() ? v1->Monde().Get<NkEmetteur2D>(feuV1) : nullptr;
				const NkEclairage2D &ec1 = v1->Eclairage();
				Temoin(luV1 && l1 != nullptr && l1->type == NkTypeLumiere2D::NK_SPOT && l1->couleur == 0x11223344u &&
						   l1->portee == 6.5f && !l1->actif && e1 != nullptr && e1->graine == 4242u && e1->maxParticules == 160u &&
						   e1->zone == NkZoneEmission2D::NK_DISQUE && ec1.actif && ec1.ambiante == 0x10203040u &&
						   ec1.mode == NkModeEclairage2D::NK_VOILE && ec1.maille == 16.f,
					   "(S5) fichier v1 AVEC eclairage : lumiere, emetteur et reglage relus", 0.f);
				if (!luV1) {
					std::printf("    erreur : %s\n", err.CStr());
				}

				// (S6) un fichier v2 SANS eclairage, ecrit par le moteur fusionne.
				NkSceneTas v2a;
				const ecs::NkEntityId sp2 = v2a->Creer("Caisse", NkVec2f(1.f, 1.f));
				NkSprite2D sp2s;
				sp2s.couleur = 0x336699FFu;
				v2a->Monde().Add<NkSprite2D>(sp2, sp2s);
				v2a->Creer("Vide", NkVec2f(-1.f, 0.f));
				NkString jsonV2;
				NkSauverSceneJSON(*v2a, jsonV2);
				NkSceneTas v2b;
				const bool luV2 = NkChargerSceneJSON(*v2b, jsonV2.View(), nullptr, &err);
				NkString jsonV2b;
				NkSauverSceneJSON(*v2b, jsonV2b);
				NkVector<ecs::NkEntityId> ids2;
				v2b->Entites(ids2);
				const bool estV2 = std::strstr(jsonV2.CStr(), "\"version\": 2") != nullptr ||
								   std::strstr(jsonV2.CStr(), "\"version\":2") != nullptr;
				Temoin(estV2 && luV2 && !v2b->Eclairage().actif && ids2.Size() == 2u && ParNom(*v2b, "Caisse").IsValid() &&
						   SansNouvellesCles(jsonV2) && SansNouvellesCles(jsonV2b),
					   "(S6) fichier v2 SANS eclairage : relu eteint, aucune nouvelle cle", static_cast<float32>(jsonV2.Length()));
			}

			void BancPhoto() {
				NkSceneTas s;
				s->Eclairage().actif = true;
				s->Eclairage().ambiante = 0x303030FFu;
				const ecs::NkEntityId el = Lumiere(*s, NkVec2f(0.f, 0.f), Blanche(3.f));
				const ecs::NkEntityId ee = Emetteur(*s, NkVec2f(1.f, 0.f), NkPresetEmetteur2D(NkPresetEffet2D::NK_FEU));
				(void)el;
				(void)ee;
				NkScene::NkPhoto photo;
				s->Photographier(photo);
				// « En jeu » : la nuit tombe, la lampe baisse, le feu s'eteint.
				s->Eclairage().ambiante = 0x000000FFu;
				s->Monde().Query<NkLumiere2D>().ForEach([](ecs::NkEntityId, NkLumiere2D &l) { l.intensite = 0.1f; });
				NkVector<ecs::NkEntityId> feux;
				s->Monde().Query<NkEmetteur2D>().ForEach([&feux](ecs::NkEntityId id, NkEmetteur2D &) { feux.PushBack(id); });
				for (int32 k = 0; k < 30; ++k) {
					s->Pas(1.f / 60.f);
				}
				for (uint32 i = 0; i < feux.Size(); ++i) {
					s->Monde().Remove<NkEmetteur2D>(feux[i]);
				}
				const uint32 enJeu = s->Effets().NbParticules();
				s->Restaurer(photo);
				float32 intensite = 0.f;
				int32 nbFeux = 0;
				s->Monde().Query<NkLumiere2D>().ForEach([&](ecs::NkEntityId, NkLumiere2D &l) { intensite = l.intensite; });
				s->Monde().Query<NkEmetteur2D>().ForEach([&](ecs::NkEntityId, NkEmetteur2D &) { ++nbFeux; });
				Temoin(enJeu > 0u && s->Eclairage().ambiante == 0x303030FFu && intensite == 1.f && nbFeux == 1 &&
						   s->Effets().NbParticules() == 0u,
					   "(S3) Arreter : ambiante, lumiere et feu reviennent ; particules a zero", static_cast<float32>(enJeu));
			}

			/// La contre-epreuve sur un VRAI fichier d'avant (hors banc : variables).
			void BancFichierAncien() {
				const char *entree = env::GetEnvVar("NK_BANC_SCENE_ANCIENNE");
				if (entree == nullptr || entree[0] == '\0') {
					std::printf("  [SAUTE] (S2f) NK_BANC_SCENE_ANCIENNE absent : pas de fichier ancien a relire\n");
					return;
				}
				NkTextures2D tex;
				NkRessourcesSim res;
				NkCreerRessourcesSim(res, &tex, nullptr); // les textures de l'editeur, par leur nom
				NkSceneTas s;
				NkString err;
				const bool lu = NkChargerSceneFichier(*s, entree, &tex, &err);
				NkString json;
				NkSauverSceneJSON(*s, json, &tex);
				const char *sortie = env::GetEnvVar("NK_BANC_SCENE_REECRITE");
				if (sortie != nullptr && sortie[0] != '\0') {
					NkFile::WriteAllText(sortie, json.CStr());
				}
				NkVector<ecs::NkEntityId> ids;
				s->Entites(ids);
				Temoin(lu && ids.Size() > 0u && !s->Eclairage().actif && SansNouvellesCles(json),
					   "(S2f) le VRAI fichier d'avant : relu, eteint, reecrit sans nouvelle cle", static_cast<float32>(ids.Size()));
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}
			}
		} // namespace

		int32 NkUnkenyLancerBancLumiere() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("\nUnkeny — banc de l'eclairage 2D et des effets\n\n");
			BancEteint();
			BancAmbianteEtPortee();
			BancOmbres();
			BancConeDirectionnelleLanterne();
			BancVoileEtCout();
			BancDeterminisme();
			BancPlafonds();
			BancPresets();
			BancCouleurEtSimulation();
			BancFeuQuiEclaire();
			BancDestruction();
			BancSauvegarde();
			BancPhoto();
			BancFichierAncien();
			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC LUMIERE REUSSI" : "BANC LUMIERE EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
