// -----------------------------------------------------------------------------
// @File    NKScenaRendu.cpp
// @Brief   Le rendu de la sequence sans fenetre (voir NKScenaRendu.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// ⚠️ L'ORDRE DES INCLUSIONS COMPTE : NKRHI et NKRenderer AVANT tout en-tete de
//    Noge qui ouvre `using namespace ecs;` (NkSequencer.h, via le modele) --
//    sinon NKRHI cesse de compiler dans ses propres en-tetes (NkRect2D ambigu,
//    mesure du 13/09, Engine/Noge/ROADMAP.md).
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

#include "NKScena/NKScenaRendu.h"
#include "NKScena/NKScenaModele.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKMemory/NKMemory.h"
#include "Noge/ECS/Systems/NkRenderSystem.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"

#include <cstring>

namespace nkentseu {
	namespace nkscena {

		using namespace renderer;

		namespace {
			uint64 Empreinte(const uint8 *p, usize n) {
				uint64 h = 1469598103934665603ull;
				for (usize i = 0; i < n; ++i) {
					h ^= (uint64)p[i];
					h *= 1099511628211ull;
				}
				return h;
			}

			const char *NomApi(NkGraphicsApi a) {
				switch (a) {
					case NkGraphicsApi::NK_GFX_API_DX11:
						return "DirectX 11";
					case NkGraphicsApi::NK_GFX_API_DX12:
						return "DirectX 12";
					case NkGraphicsApi::NK_GFX_API_VULKAN:
						return "Vulkan";
					case NkGraphicsApi::NK_GFX_API_OPENGL:
						return "OpenGL";
					default:
						return "autre";
				}
			}

			NkIDevice *OuvrirSansSurface(NkGraphicsApi api) {
				NkDeviceInitInfo di;
				di.api = api;
				di.width = 0; // pas de surface : sans fenetre
				di.height = 0;
				di.context.software.threading = true;
				NkIDevice *dev = NkDeviceFactory::Create(di);
				if (dev != nullptr && (!dev->IsValid() || dev->GetApi() != api)) {
					NkDeviceFactory::Destroy(dev);
					dev = nullptr;
				}
				return dev;
			}
		} // namespace

		void NkScenaRetournerLignes(uint8 *px, uint32 w, uint32 h) {
			if (px == nullptr || w == 0u || h < 2u) {
				return;
			}
			const usize ligne = (usize)w * 4u;
			uint8 *tampon = (uint8 *)memory::NkAlloc(ligne);
			if (tampon == nullptr) {
				return;
			}
			for (uint32 y = 0; y < h / 2u; ++y) {
				uint8 *a = px + (usize)y * ligne;
				uint8 *b = px + (usize)(h - 1u - y) * ligne;
				std::memcpy(tampon, a, ligne);
				std::memcpy(a, b, ligne);
				std::memcpy(b, tampon, ligne);
			}
			memory::NkFree(tampon);
		}

		int32 NkScenaRendreSansFenetre(NkScenaModele &m, const NkScenaRenduDesc &d, NkScenaRenduResultat &r) {
			r = NkScenaRenduResultat{};
			if (!m.Pret()) {
				r.raison = "pas de scene";
				return 0;
			}
			// ── Le peripherique, SANS SURFACE ────────────────────────────────
#if defined(_WIN32)
			NkIDevice *dev = OuvrirSansSurface(NkGraphicsApi::NK_GFX_API_DX11);
			if (dev == nullptr) {
				dev = OuvrirSansSurface(NkGraphicsApi::NK_GFX_API_OPENGL);
			}
#else
			NkIDevice *dev = OuvrirSansSurface(NkGraphicsApi::NK_GFX_API_OPENGL);
#endif
			if (dev == nullptr) {
				r.raison = "aucun peripherique graphique";
				return -1;
			}
			r.api = NomApi(dev->GetApi());
			const uint32 W = d.largeur < 16u ? 16u : d.largeur;
			const uint32 H = d.hauteur < 16u ? 16u : d.hauteur;

			// ── Le renderer, sortie redirigee vers la cible hors ecran (le chemin de
			//    NogeeVue3D, ici sans application autour) ─────────────────────
			NkRendererConfig cfg;
			cfg.api = dev->GetApi();
			cfg.width = W;
			cfg.height = H;
			cfg.vsync = false;
			cfg.Enable(NK_SS_OFFSCREEN);
			NkRenderer *rendu = NkRenderer::Create(dev, cfg);
			if (rendu == nullptr) {
				r.raison = "NkRenderer refuse";
				NkDeviceFactory::Destroy(dev);
				return 0;
			}
			NkOffscreenDesc od;
			od.width = W;
			od.height = H;
			od.hasDepth = true;
			od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
			od.readable = true;
			od.readback = true;
			od.name = "NKScenaRendu";
			NkOffscreenTarget *cible = rendu->CreateOffscreen(od);
			if (cible == nullptr || !cible->IsValid()) {
				r.raison = "cible hors ecran refusee";
				NkRenderer::Destroy(rendu);
				NkDeviceFactory::Destroy(dev);
				return 0;
			}
			rendu->SetRenderSizeOverride(W, H);
			if (NkTextureLibrary *tex = rendu->GetTextures()) {
				rendu->SetFinalColorTarget(tex->GetRHIHandle(cible->GetColorResult()));
			}
			rendu->SetBackgroundColor({0.13f, 0.14f, 0.16f, 1.f});

			// ── Les systemes de Noge, ceux de NkEngineLayer ──────────────────
			// NkRenderSystem exige un command buffer non nul a l'initialisation ;
			// celui de la trame lui est donne a chaque image (SetCommandBuffer).
			NkICommandBuffer *factice = dev->CreateCommandBuffer();
			NkTransformSystem transforms;
			NkRenderSystem systemeRendu;
			systemeRendu.Init(rendu, factice);
			ecs::NkWorld &monde = m.scene.Monde();
			// ⚠️ NkRenderSystem GARDE dans chaque NkMeshComponent la poignee de
			//    maillage de SON renderer. Celui-ci est neuf (et sera detruit) : les
			//    poignees d'un autre rendu ne valent rien ici, et les siennes ne
			//    vaudront plus rien apres lui -- elles sont remises a zero des deux
			//    cotes.
			auto OublierMaillages = [&]() {
				monde.Query<ecs::NkMeshComponent>().ForEach([](ecs::NkEntityId, ecs::NkMeshComponent &mc) { mc.meshHandle = 0; });
			};
			OublierMaillages();

			// ── Les instants ─────────────────────────────────────────────────
			const float32 ips = m.frise.fps > 0.f ? m.frise.fps : 24.f;
			const float32 debut = m.frise.PlayStart();
			const float32 fin = m.frise.PlayEnd();
			int32 n = d.images;
			if (n <= 0) {
				n = (int32)((fin - debut) * ips + 0.5f);
				n = n < 1 ? 1 : n;
			}
			const bool vueAvant = m.vueCamera;
			m.vueCamera = true; // le film est ce que voit la camera du plan

			media::NkImageSequenceWriter ecrivain;
			bool ouvert = false;
			if (d.ecrire) {
				(void)NkDirectory::CreateRecursive(d.dossier.CStr());
				ouvert = ecrivain.Open(d.dossier.CStr(), d.prefixe.CStr(), (int32)W, (int32)H, media::NkImageSeqFormat::PNG, 4);
				if (!ouvert) {
					r.raison = "dossier de sortie refuse";
				}
			}
			const usize octets = (usize)W * H * 4u;
			uint8 *px = (uint8 *)memory::NkAlloc(octets);
			const bool basEnHaut = NkOffscreenStoredIsBottomUp(dev->GetApi());
			for (int32 i = 0; i < n && px != nullptr && (ouvert || !d.ecrire); ++i) {
				const float32 t = d.figer ? debut : debut + (float32)i / ips;
				m.Evaluer(t);
				// Les matrices monde AVANT le rendu : sans ce pas, la pose ecrite par
				// la sequence ne serait vue qu'a l'image suivante.
				transforms.Execute(monde, 1.f / ips);
				if (!rendu->BeginFrame()) {
					r.raison = "BeginFrame refuse";
					break;
				}
				systemeRendu.SetCommandBuffer(rendu->GetCmd());
				systemeRendu.Execute(monde, 1.f / ips);
				rendu->Present();
				rendu->EndFrame();
				dev->WaitIdle();
				if (!cible->ReadbackPixels(px, W * 4u)) {
					r.raison = "relecture refusee";
					break;
				}
				if (basEnHaut) {
					NkScenaRetournerLignes(px, W, H);
				}
				++r.rendues;
				const uint64 h = Empreinte(px, octets);
				if (i == 0) {
					r.premiere = h;
				}
				r.derniere = h;
				if (d.ecrire && ecrivain.WriteFrame(px, media::NkVideoInputFormat::RGBA32)) {
					++r.ecrites;
				}
			}
			if (px != nullptr) {
				// Ce que la derniere image montre : combien de pixels sont le fond.
				r.pixels = W * H;
				for (uint32 k = 0; k < W * H; ++k) {
					const uint8 *q = px + (usize)k * 4u;
					r.pixelsFond += (q[0] == px[0] && q[1] == px[1] && q[2] == px[2]) ? 1u : 0u;
				}
				memory::NkFree(px);
			}
			ecrivain.Close();
			m.vueCamera = vueAvant;
			m.Evaluer(m.frise.cursor);
			OublierMaillages();

			dev->WaitIdle();
			if (factice != nullptr) {
				dev->DestroyCommandBuffer(factice);
			}
			rendu->SetFinalColorTarget(NkTextureHandle{});
			rendu->DestroyOffscreen(cible);
			NkRenderer::Destroy(rendu);
			NkDeviceFactory::Destroy(dev);
			return r.rendues == n ? 1 : 0;
		}

	} // namespace nkscena
} // namespace nkentseu
