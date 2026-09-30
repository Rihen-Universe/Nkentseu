//
// NogeDemoApp.cpp
// =============================================================================
// Description :
//   La fenetre de NogeDemo et sa capture hors ecran (voir NogeDemoApp.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NogeDemoApp.h"

#include "Noge/Layers/NkEngineLayer.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/NkRenderer.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogedemo {

		using namespace ecs;
		using namespace math;

		namespace {

			/// Cree le dossier qui contient `chemin` (s'il y en a un).
			void PreparerDossier(const char *chemin) {
				if (chemin == nullptr) {
					return;
				}
				char dossier[512];
				std::snprintf(dossier, sizeof(dossier), "%s", chemin);
				char *fin = nullptr;
				for (char *c = dossier; *c != '\0'; ++c) {
					if (*c == '/' || *c == '\\') {
						fin = c;
					}
				}
				if (fin != nullptr) {
					*fin = '\0';
					if (!NkDirectory::Exists(dossier)) {
						(void)NkDirectory::CreateRecursive(dossier);
					}
				}
			}

			/// Projette un point monde en pixels (origine en HAUT a gauche).
			/// false s'il est derriere la camera.
			bool Projeter(const NkMat4f &m, const NkVec3f &p, uint32 w, uint32 h, float32 &sx, float32 &sy) {
				const float32 cx = m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z + m[3][0];
				const float32 cy = m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z + m[3][1];
				const float32 cw = m[0][3] * p.x + m[1][3] * p.y + m[2][3] * p.z + m[3][3];
				if (cw <= 1e-5f) {
					return false;
				}
				sx = (cx / cw * 0.5f + 0.5f) * static_cast<float32>(w);
				sy = (1.f - (cy / cw * 0.5f + 0.5f)) * static_cast<float32>(h);
				return true;
			}

			// Les familles de couleur, en octets sRGB (apres tonemapping). Seuils
			// larges : on reconnait un ROUGE et un JAUNE, on ne mesure pas une teinte.
			bool EstRouge(const uint8 *px) {
				const int32 r = px[0], g = px[1], b = px[2];
				return r > 90 && r > g * 2 && r > b * 2;
			}

			bool EstJaune(const uint8 *px) {
				const int32 r = px[0], g = px[1], b = px[2];
				return r > 90 && g > 70 && b * 2 < g && b * 2 < r;
			}

			struct NkAmas {
					int32 n = 0;
					float32 cx = 0.f;
					float32 cy = 0.f;
			};

			template <typename F> NkAmas Amas(const NkVector<uint8> &img, uint32 w, uint32 h, F estDeLaFamille) {
				NkAmas a;
				double sx = 0.0;
				double sy = 0.0;
				for (uint32 y = 0; y < h; ++y) {
					for (uint32 x = 0; x < w; ++x) {
						if (estDeLaFamille(img.Data() + (static_cast<nk_usize>(y) * w + x) * 4u)) {
							++a.n;
							sx += x;
							sy += y;
						}
					}
				}
				if (a.n > 0) {
					a.cx = static_cast<float32>(sx / a.n);
					a.cy = static_cast<float32>(sy / a.n);
				}
				return a;
			}

			int32 gReussis = 0;
			int32 gEchecs = 0;

			void Temoin(bool ok, const char *quoi, const char *mesure) {
				(ok ? gReussis : gEchecs)++;
				std::printf("  [%s] %s  (%s)\n", ok ? "OK" : "ECHEC", quoi, mesure);
			}

		} // namespace

		// =====================================================================
		NogeDemoApp::NogeDemoApp(const NkApplicationConfig &config, const NogeDemoOptions &options)
			: NkApplication(config), mOptions(options) {
		}

		// =====================================================================
		void NogeDemoApp::OnInit() {
			mCouche = new NkEngineLayer();
			PushLayer(mCouche); // OnAttach : rendu, systemes, ordonnanceur

			// --- Les entrees : le fichier s'il existe, sinon le texte par defaut
			// ECRIT a sa place -- pour qu'on puisse le modifier.
			NkInputMap &carte = mCouche->GetInput();
			const char *chemin = mOptions.cheminEntrees.CStr();
			NkString texte(NogeDemoTexteEntrees());
			if (NkFile::Exists(chemin)) {
				texte = NkFile::ReadAllText(chemin);
			} else if (mOptions.capture.Empty()) {
				PreparerDossier(chemin);
				(void)NkFile::WriteAllText(chemin, NogeDemoTexteEntrees());
			}
			NkString erreur;
			if (!NogeDemoDeclarerEntrees(carte, mEtat, texte.CStr(), &erreur)) {
				logger.Errorf("[NogeDemo] entrees refusees ({0}) : {1} -- le texte par defaut est pris\n", chemin,
							  erreur.CStr());
				carte.Clear();
				(void)NogeDemoDeclarerEntrees(carte, mEtat, NogeDemoTexteEntrees(), nullptr);
			}

			// En capture, AUCUNE vraie entree n'est lue : la fenetre est hors
			// ecran, mais une frappe qui y arriverait quand meme ne doit rien
			// fabriquer dans la mesure.
			if (!mOptions.capture.Empty()) {
				carte.SetAcceptKeyboardMouse(false);
				carte.SetAcceptTouch(false);
				carte.SetGamepad(NK_INPUT_NO_GAMEPAD);
			}

			// --- La scene ---------------------------------------------------
			NkWorld &monde = mCouche->GetWorld();
			bool charge = false;
			if (mOptions.chargerAuDepart) {
				charge = NogeDemoCharger(monde, mCouche->GetPhysicsSystem(), mOptions.cheminScene.CStr(), mEtat);
			}
			if (!charge) {
				NogeDemoConstruireScene(monde);
			}

			std::printf("[NogeDemo] Noge : premier resultat -- %s\n", charge ? "scene RELUE" : "scene neuve");
			std::printf("[NogeDemo] ZQSD (WASD) pousser le cube | Espace sauter | E lacher une caisse\n"
						"[NogeDemo] clic droit + souris, ou fleches : tourner la camera | molette : zoom\n"
						"[NogeDemo] F5 sauver | F9 recharger | Echap quitter | entrees : %s\n",
						chemin);
			std::fflush(stdout);
		}

		// =====================================================================
		void NogeDemoApp::OnUpdate(float dt) {
			if (mCouche == nullptr) {
				return;
			}
			mTemps += dt;
			// Les actions de CETTE image : NkEngineLayer::OnUpdate a deja avance
			// la carte (il passe avant l'application dans NkApplication::Run).
			NogeDemoPas(mEtat, mCouche->GetWorld(), mCouche->GetInput(), dt);
			ExecuterDemandes();
			if (!mOptions.capture.Empty()) {
				PiloterCapture();
			}
		}

		// =====================================================================
		void NogeDemoApp::ExecuterDemandes() {
			NkWorld &monde = mCouche->GetWorld();
			if (mEtat.demandeSauver) {
				mEtat.demandeSauver = false;
				PreparerDossier(mOptions.cheminScene.CStr());
				const bool ok = NogeDemoSauver(monde, mOptions.cheminScene.CStr());
				std::printf("[NogeDemo] F5 : %s %s\n", ok ? "scene sauvee dans" : "ECHEC de la sauvegarde de",
							mOptions.cheminScene.CStr());
				GetWindow().SetTitle(ok ? NkString("NogeDemo -- scene sauvee (F9 pour la recharger)")
										: NkString("NogeDemo -- ECHEC de la sauvegarde"));
			}
			if (mEtat.demandeCharger) {
				mEtat.demandeCharger = false;
				const bool ok = NogeDemoCharger(monde, mCouche->GetPhysicsSystem(), mOptions.cheminScene.CStr(), mEtat);
				std::printf("[NogeDemo] F9 : %s %s\n", ok ? "scene rechargee depuis" : "rien a recharger depuis",
							mOptions.cheminScene.CStr());
				GetWindow().SetTitle(ok ? NkString("NogeDemo -- scene rechargee")
										: NkString("NogeDemo -- aucune scene sauvee (F5 d'abord)"));
			}
			if (mEtat.demandeQuitter) {
				mEtat.demandeQuitter = false;
				Quit();
			}
			std::fflush(stdout);
		}

		// =====================================================================
		// La capture : armer la cible, laisser 4 images s'y rendre, LIRE ; cacher
		// le joueur, 4 images, relire ; comparer. Le second releve est la
		// CONTRE-EPREUVE : sans le cube, le rouge doit disparaitre -- il venait
		// donc bien de son dessin, pas d'une coincidence de projection.
		// =====================================================================
		void NogeDemoApp::PiloterCapture() {
			renderer::NkRenderer *r = GetRenderer();
			NkIDevice *device = GetDevice();
			if (r == nullptr || device == nullptr) {
				std::printf("[NogeDemo] capture impossible : pas de renderer\n");
				mCodeSortie = 2;
				Quit();
				return;
			}
			const NkEntityId joueur = NogeDemoTrouver(mCouche->GetWorld(), kNomJoueur);
			NkMeshComponent *mesh = joueur.IsValid() ? mCouche->GetWorld().Get<NkMeshComponent>(joueur) : nullptr;
			// ⚠️ ReadbackPixels rend DEJA l'image de haut en bas (il retourne
			//    lui-meme les rangees OpenGL, NkOffscreenTarget.cpp) : la
			//    retourner ici une seconde fois lisait le sol en haut de l'image
			//    (mesure du 30/09, premiere capture).
			auto lire = [&](NkVector<uint8> &dst) -> bool {
				device->WaitIdle();
				mLargeur = mCible.GetWidth();
				mHauteur = mCible.GetHeight();
				dst.Resize(static_cast<nk_usize>(mLargeur) * mHauteur * 4u);
				return mCible.ReadbackPixels(dst.Data());
			};

			switch (mPhase) {
				case 0: {
					if (mTemps < mOptions.tempsCapture) {
						return;
					}
					renderer::NkOffscreenDesc od;
					od.width = r->GetWidth();
					od.height = r->GetHeight();
					od.hasDepth = false;
					od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
					od.readback = true;
					od.name = "nogedemo_capture";
					if (!mCible.Init(device, r->GetTextures(), od)) {
						std::printf("[NogeDemo] capture : cible hors ecran refusee\n");
						mCodeSortie = 2;
						Quit();
						return;
					}
					r->SetFinalColorTarget(r->GetTextures()->GetRHIHandle(mCible.GetColorResult()));
					mPhase = 1;
					mImagesDepuisPhase = 0;
					return;
				}
				case 1: {
					if (++mImagesDepuisPhase < 4) {
						return;
					}
					const bool lu = lire(mPixelsVisible);
					const bool ecrit = mCible.Capture(mOptions.capture.CStr());
					mVueProj = mCouche->GetRenderSystem().GetViewProjection();
					std::printf("[NogeDemo] capture : %s -> %s (%ux%u), %u draw calls soumis par NkRenderSystem\n",
								lu ? "relue" : "RELECTURE ECHOUEE", ecrit ? mOptions.capture.CStr() : "IMAGE NON ECRITE",
								mLargeur, mHauteur, mCouche->GetRenderSystem().GetLastSubmittedCount());
					if (mesh != nullptr) {
						mesh->visible = false; // la contre-epreuve
					}
					mPhase = 2;
					mImagesDepuisPhase = 0;
					return;
				}
				case 2: {
					if (++mImagesDepuisPhase < 4) {
						return;
					}
					(void)lire(mPixelsCache);
					if (mesh != nullptr) {
						mesh->visible = true;
					}
					r->SetFinalColorTarget(NkTextureHandle{});
					mCible.Shutdown();
					VerifierCapture();
					mPhase = 4;
					Quit();
					return;
				}
				default:
					return;
			}
		}

		// =====================================================================
		void NogeDemoApp::VerifierCapture() {
			gReussis = 0;
			gEchecs = 0;
			std::printf("\nNogeDemo -- banc de la capture (le rendu du moteur, relu en pixels)\n");
			const uint32 w = mLargeur;
			const uint32 h = mHauteur;
			char m[160];
			if (w == 0 || h == 0 || mPixelsVisible.Size() != static_cast<nk_usize>(w) * h * 4u ||
				mPixelsCache.Size() != mPixelsVisible.Size()) {
				Temoin(false, "(c0) les deux images sont relues", "relecture vide");
				std::printf("\nBANC CAPTURE EN ECHEC : %d reussis, %d echec%s\n", gReussis, gEchecs, gEchecs > 1 ? "s" : "");
				mCodeSortie = 1;
				return;
			}
			NkWorld &monde = mCouche->GetWorld();
			auto position = [&](const char *nom) -> NkVec3f {
				const NkEntityId id = NogeDemoTrouver(monde, nom);
				const NkTransform *tf = id.IsValid() ? monde.Get<NkTransform>(id) : nullptr;
				return tf != nullptr ? tf->localPosition : NkVec3f{0.f, 0.f, 0.f};
			};

			// (c1) le cube rouge est DESSINE, la ou la camera le voit.
			const NkAmas rouge = Amas(mPixelsVisible, w, h, EstRouge);
			float32 jx = 0.f;
			float32 jy = 0.f;
			const bool jVu = Projeter(mVueProj, position(kNomJoueur), w, h, jx, jy);
			const float32 ecartJ = NkSqrt((rouge.cx - jx) * (rouge.cx - jx) + (rouge.cy - jy) * (rouge.cy - jy));
			std::snprintf(m, sizeof(m), "%d px rouges, centre (%.0f, %.0f), projete (%.0f, %.0f), ecart %.1f px", rouge.n,
						  rouge.cx, rouge.cy, jx, jy, ecartJ);
			Temoin(jVu && rouge.n > 200 && ecartJ < 0.05f * static_cast<float32>(h),
				   "(c1) le cube rouge est rendu a sa place (amas de rouge centre sur sa projection)", m);

			// (c1n) CONTRE-EPREUVE : le cube cache, le rouge disparait.
			const NkAmas rougeCache = Amas(mPixelsCache, w, h, EstRouge);
			std::snprintf(m, sizeof(m), "%d px rouges sans le cube (contre %d avec)", rougeCache.n, rouge.n);
			Temoin(rougeCache.n * 20 < rouge.n, "(c1n) contre-epreuve : cube cache, plus de rouge", m);

			// (c2) la balle jaune, posee SUR le cube.
			const NkAmas jaune = Amas(mPixelsVisible, w, h, EstJaune);
			float32 bx = 0.f;
			float32 by = 0.f;
			const bool bVu = Projeter(mVueProj, position(kNomBalle), w, h, bx, by);
			const float32 ecartB = NkSqrt((jaune.cx - bx) * (jaune.cx - bx) + (jaune.cy - by) * (jaune.cy - by));
			std::snprintf(m, sizeof(m), "%d px jaunes, centre (%.0f, %.0f), projete (%.0f, %.0f), ecart %.1f px", jaune.n,
						  jaune.cx, jaune.cy, bx, by, ecartB);
			Temoin(bVu && jaune.n > 100 && ecartB < 0.05f * static_cast<float32>(h),
				   "(c2) la balle jaune est rendue a sa place", m);
			// La balle est AU-DESSUS du cube a l'ecran (y ecran plus petit).
			std::snprintf(m, sizeof(m), "y balle %.0f, y cube %.0f (ecran, vers le bas)", jaune.cy, rouge.cy);
			Temoin(jaune.n > 0 && rouge.n > 0 && jaune.cy < rouge.cy, "(c3) a l'image, la balle est posee SUR le cube", m);

			// (c4) le sol : un point du sol, loin des objets, n'est ni le fond ni
			// un objet ; le haut de l'image (ciel) n'est pas le sol.
			float32 sx = 0.f;
			float32 sy = 0.f;
			const bool sVu = Projeter(mVueProj, NkVec3f{-4.f, 0.f, 4.f}, w, h, sx, sy);
			const uint32 ix = sVu ? static_cast<uint32>(NkClamp(sx, 0.f, static_cast<float32>(w - 1))) : 0u;
			const uint32 iy = sVu ? static_cast<uint32>(NkClamp(sy, 0.f, static_cast<float32>(h - 1))) : 0u;
			const uint8 *sol = mPixelsVisible.Data() + (static_cast<nk_usize>(iy) * w + ix) * 4u;
			const uint8 *ciel = mPixelsVisible.Data() + (static_cast<nk_usize>(2) * w + w / 2) * 4u;
			const int32 dr = static_cast<int32>(sol[0]) - ciel[0];
			const int32 dg = static_cast<int32>(sol[1]) - ciel[1];
			const int32 db = static_cast<int32>(sol[2]) - ciel[2];
			const int32 dist = dr * dr + dg * dg + db * db;
			std::snprintf(m, sizeof(m), "sol (%u,%u) = %u,%u,%u ; haut = %u,%u,%u", ix, iy, sol[0], sol[1], sol[2],
						  ciel[0], ciel[1], ciel[2]);
			Temoin(sVu && !EstRouge(sol) && !EstJaune(sol) && dist > 20 * 20,
				   "(c4) le sol est rendu (un point du sol n'est ni un objet ni le haut de l'image)", m);

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC CAPTURE REUSSI" : "BANC CAPTURE EN ECHEC",
						gReussis, gEchecs, gEchecs > 1 ? "s" : "");
			std::fflush(stdout);
			mCodeSortie = gEchecs == 0 ? 0 : 1;
		}

		// =====================================================================
		void NogeDemoApp::OnShutdown() {
			// La cible hors ecran AVANT le device (NkApplication le detruit juste apres).
			if (mCible.IsValid()) {
				if (GetRenderer() != nullptr) {
					GetRenderer()->SetFinalColorTarget(NkTextureHandle{});
				}
				mCible.Shutdown();
			}
		}

	} // namespace nogedemo
} // namespace nkentseu
