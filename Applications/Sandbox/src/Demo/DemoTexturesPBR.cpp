// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// DemoTexturesPBR.cpp — LA SCENE QUI CHARGE VRAIMENT DES TEXTURES
//
// Elle existe pour une raison precise, et c'est une raison de MESURE.
//
// `renderdemo --demo=2` ne charge qu'UN PNG de 256x256 : 1 ms. Mesurer le format
// d'actif sur lui aurait donne « le gain est negligeable » — un chiffre juste sur
// le mauvais sujet. La charge reelle du moteur, c'est un materiau PBR complet ou
// un modele importe : dix cartes, 2048 et 4096 pixels de cote, 55 Mo de disque.
// Cette demo charge exactement ça, par le chemin NORMAL du moteur
// (`NkTextureLibrary::Load`), et imprime ce que ça a coute.
//
//   renderdemo --demo=20                      # cache actif (defaut)
//   NK_TEX_CACHE=0 renderdemo --demo=20       # sans cache : le « avant »
//   NK_MAXFRAMES=8 renderdemo --demo=20       # course courte, sortie propre
//
// La ligne « [TexturesPBR] CHARGEMENT » est le chiffre du lot : c'est le meme
// binaire, la meme scene, le meme GPU des deux cotes — seul l'interrupteur
// change. Les compteurs touches/manques/refus disent POURQUOI le chiffre est ce
// qu'il est : un cache sature et un cache jamais consulte ont le meme temps de
// service, et sans eux on ne saurait pas lequel on regarde.
// =============================================================================
#include "DemoCommon.h"

#include "NKRenderer/Core/NkTextureCache.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace demo {

		namespace {

			struct Carte {
					const char *chemin;
					bool couleur; // sRGB pour l'albedo, lineaire pour les donnees
			};

			// Le corpus reel du depot. Cinq cartes d'un materiau PBR (2048) et
			// cinq d'un modele importe (4096) : c'est ce qu'une scene de Noge
			// charge des qu'elle porte autre chose qu'une couleur unie.
			const Carte kCartes[] = {
				{"Resources/Textures/PBR/rusted_iron/albedo.png", true},
				{"Resources/Textures/PBR/rusted_iron/normal.png", false},
				{"Resources/Textures/PBR/rusted_iron/metallic.png", false},
				{"Resources/Textures/PBR/rusted_iron/roughness.png", false},
				{"Resources/Textures/PBR/rusted_iron/ao.png", false},
				{"Resources/Models/backpack/diffuse.jpg", true},
				{"Resources/Models/backpack/normal.png", false},
				{"Resources/Models/backpack/specular.jpg", false},
				{"Resources/Models/backpack/roughness.jpg", false},
				{"Resources/Models/backpack/ao.jpg", false},
			};
			constexpr int kNbCartes = (int)(sizeof(kCartes) / sizeof(kCartes[0]));

		} // namespace

		struct DemoTexturesPBRState {
				NkTexHandle textures[kNbCartes];
				int chargees = 0;
				float64 msChargement = 0.0;
				nk_uint32 touches = 0, manques = 0, refus = 0;
				float64 msLecture = 0.0, msDecodage = 0.0, msTelev = 0.0;
				nk_uint64 octetsLus = 0;
				bool cacheActif = true;
				uint64 vramEstimee = 0;
		};

		bool DemoTexturesPBR_Init(DemoCtx &ctx) {
			auto *st = new DemoTexturesPBRState();
			ctx.userData = st;

			auto *texLib = ctx.renderer->GetTextures();
			if (!texLib) {
				logger.Errorf("[TexturesPBR] TextureLibrary manquant\n");
				delete st;
				ctx.userData = nullptr;
				return false;
			}

			st->cacheActif = NkTextureCache::Actif();
			NkTextureCache::RemettreCompteursAZero();

			logger.Info("[TexturesPBR] cache d'actifs : {0}\n", st->cacheActif ? "ACTIF" : "COUPE (NK_TEX_CACHE=0)");

			// Le chronometre entoure EXACTEMENT ce qu'on veut mesurer : les dix
			// appels a `Load`. Ni la creation du device, ni la fenetre, ni les
			// shaders — sinon le chiffre melangerait ce que le lot change avec ce
			// qu'il ne touche pas.
			NkChrono c;
			for (int i = 0; i < kNbCartes; ++i) {
				NkLoadOptions opts;
				opts.srgb = kCartes[i].couleur;
				opts.genMipmaps = true;
				st->textures[i] = texLib->Load(NkString(kCartes[i].chemin), opts);
				if (st->textures[i].IsValid())
					++st->chargees;
				else
					logger.Warnf("[TexturesPBR] absente ou illisible : %s\n", kCartes[i].chemin);
			}
			st->msChargement = c.Elapsed().ToMilliseconds();

			st->touches = NkTextureCache::Touches();
			st->manques = NkTextureCache::Manques();
			st->refus = NkTextureCache::Refus();
			st->msLecture = NkTextureCache::MsLecture();
			st->msDecodage = NkTextureCache::MsDecodage();
			st->msTelev = NkTextureCache::MsTeleversement();
			st->octetsLus = NkTextureCache::OctetsLus();
			st->vramEstimee = texLib->GetEstimatedVRAMBytes();

			logger.Info("[TexturesPBR] CHARGEMENT : {0} ms pour {1}/{2} cartes\n", st->msChargement, st->chargees,
						kNbCartes);
			logger.Info("[TexturesPBR] cache : {0} touche(s), {1} manque(s), {2} refus\n", st->touches, st->manques,
						st->refus);
			logger.Info("[TexturesPBR] VRAM estimee : {0} octets\n", (unsigned long long)st->vramEstimee);
			// Le PARTAGE, sans lequel le total ne designe personne.
			logger.Info("[TexturesPBR] POSTES : lecture {0} ms | decodage {1} ms | televersement {2} ms | {3} o lus\n",
						st->msLecture, st->msDecodage, st->msTelev, (unsigned long long)st->octetsLus);

			return st->chargees > 0;
		}

		void DemoTexturesPBR_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (DemoTexturesPBRState *)ctx.userData;
			if (!st)
				return;

			if (!ctx.renderer->BeginFrame())
				return;

			auto *r2d = ctx.renderer->GetRender2D();
			if (r2d) {
				r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				// Une grille : les dix cartes, vraiment echantillonnees par le
				// GPU. Une texture chargee mais jamais dessinee ne prouverait pas
				// qu'elle est televersable.
				const float32 taille = 160.f;
				const float32 marge = 14.f;
				const int parLigne = 5;
				const float32 largeurTotale = parLigne * taille + (parLigne - 1) * marge;
				const float32 x0 = ((float32)ctx.width - largeurTotale) * 0.5f;
				const float32 y0 = 150.f;
				for (int i = 0; i < kNbCartes; ++i) {
					if (!st->textures[i].IsValid())
						continue;
					const int col = i % parLigne;
					const int lig = i / parLigne;
					NkRectF r = {x0 + col * (taille + marge), y0 + lig * (taille + marge), taille, taille};
					r2d->DrawSprite(r, st->textures[i]);
				}
				r2d->End();
			}

			if (auto *overlay = ctx.renderer->GetOverlay()) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawStats(ctx.renderer->GetStats());
				overlay->DrawText({20.f, 35.f}, "DemoTexturesPBR — 10 cartes reelles  |  API : %s",
								  NkGraphicsApiName(ctx.api));
				overlay->DrawText({20.f, 55.f}, "cache d'actifs : %s", st->cacheActif ? "ACTIF" : "COUPE (NK_TEX_CACHE=0)");
				overlay->DrawText({20.f, 75.f}, "CHARGEMENT : %.1f ms  (%d/%d cartes)", st->msChargement, st->chargees,
								  kNbCartes);
				overlay->DrawText({20.f, 95.f}, "cache : %u touche(s), %u manque(s), %u refus", st->touches,
								  st->manques, st->refus);
				overlay->DrawText({20.f, 115.f}, "VRAM estimee : %.1f Mo", double(st->vramEstimee) / 1048576.0);
				overlay->DrawText({20.f, 135.f}, "postes : lecture %.0f  decodage %.0f  televersement %.0f (ms)",
								  st->msLecture, st->msDecodage, st->msTelev);
				overlay->DrawText({20.f, 155.f}, "FPS : %.0f", dt > 1e-5f ? 1.f / dt : 0.f);
				overlay->EndOverlay();
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();
		}

		void DemoTexturesPBR_Shutdown(DemoCtx &ctx) {
			auto *st = (DemoTexturesPBRState *)ctx.userData;
			if (!st)
				return;
			// Le recapitulatif est REIMPRIME a la sortie : sur une course courte
			// (`NK_MAXFRAMES`), la ligne d'init peut etre noyee dans le journal du
			// demarrage, et c'est elle qu'on vient lire.
			logger.Info("[TexturesPBR] === RECAPITULATIF ===\n");
			logger.Info("[TexturesPBR] cache {0} | chargement {1} ms | {2} touche(s) {3} manque(s) {4} refus | VRAM "
						"{5} o\n",
						st->cacheActif ? "ACTIF" : "COUPE", st->msChargement, st->touches, st->manques, st->refus,
						(unsigned long long)st->vramEstimee);
			logger.Info("[TexturesPBR] POSTES : lecture {0} ms | decodage {1} ms | televersement {2} ms | {3} o lus\n",
						st->msLecture, st->msDecodage, st->msTelev, (unsigned long long)st->octetsLus);
			delete st;
			ctx.userData = nullptr;
		}

	} // namespace demo
} // namespace nkentseu
