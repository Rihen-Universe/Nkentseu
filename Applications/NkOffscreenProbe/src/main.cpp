// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkOffscreenProbe — un périphérique RHI se crée-t-il SANS FENÊTRE, et sait-il
// rendre ? Quatre mesures, quatre dorsaux, un négatif qui compte plus que tout.
// -----------------------------------------------------------------------------
// POURQUOI CETTE SONDE EXISTE.
//
// `NkSequenceCheck` a prouvé le pilote temporel : des clés produisent 48 PNG
// numérotés dont deux diffèrent. Mais ses pixels sont peints par le processeur.
// Pour que les images viennent du VRAI rendu, il faut d'abord répondre à une
// question qu'aucune lecture ne tranche :
//
//   `NkOffscreenTarget` et le mode « headless » de NKRHI tournent aujourd'hui —
//   mais dans des applications QUI ONT UNE FENÊTRE (NK3DModeler, Sandbox).
//   Qu'un appel existe ne dit pas qu'il s'exécute ; qu'il s'exécute AVEC une
//   fenêtre ne dit pas qu'il s'exécute SANS.
//
// LES QUATRE MESURES, chacune avec son attendu écrit avant :
//
//   m1  un NkIDevice se crée-t-il sans surface ?        dev != null && IsValid()
//   m2  une cible hors écran s'initialise-t-elle ?      Init() && IsValid()
//   m3  une passe s'exécute-t-elle, et se relit-elle ?  ReadbackPixels() == true
//   m4  LA COULEUR RELUE EST-ELLE CELLE QU'ON A DEMANDEE ?
//
// ⚠️ m4 EST LE SEUL QUI COMPTE VRAIMENT, et voici pourquoi.
//
// Une image non noire ne prouve pas qu'on a rendu : `NkOffscreenTarget::Init`
// efface déjà la cible à la création (l.78-90 de son .cpp, et pour une bonne
// raison — une cible neuve contient de la mémoire GPU recyclée, on y a déjà vu
// des morceaux d'autres applications). Une relecture « qui marche » peut donc
// n'être que cet effacement-là, ou un tampon jamais écrit.
//
// Le seul test qui distingue « la passe s'est exécutée » de « la relecture me
// raconte n'importe quoi » est de demander DEUX EFFACEMENTS DIFFÉRENTS et
// d'exiger DEUX RELECTURES DIFFÉRENTES. Si deux couleurs opposées ramènent la
// même chose, ce n'est pas le dorsal qui échoue, c'est l'instrument qui ment —
// et tout ce qu'on bâtirait dessus serait faux.
//
// ⚠️ AUCUNE FENÊTRE VISIBLE N'EST OUVERTE PAR CETTE SONDE. Sur OpenGL seulement,
// NKRHI crée lui-même une fenêtre CACHÉE de 1×1 (`NkOpenglDevice.cpp:388-399`,
// WS_POPUP, détruite au Shutdown) parce que GL exige une surface pour son
// contexte. Ce n'est pas moi qui l'ouvre et je ne la ferme pas à la main — mais
// je ne prétendrai pas que zéro fenêtre a été créée. C'est écrit dans la sortie.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>

#if defined(_WIN32)
#	include <windows.h>
#endif

using namespace nkentseu;

namespace {

	const uint32 kW = 64, kH = 64; // petit : on mesure une couleur, pas une scène

	int gPass = 0, gFail = 0, gDorsauxOk = 0;

	void Verdict(const char *dorsal, const char *nom, bool ok, const char *detail) {
		if (ok)
			gPass++;
		else
			gFail++;
		std::printf("  [%s] %-8s %-40s %s\n", ok ? "OK " : "NON", dorsal, nom, detail);
	}

	struct Pixel {
			uint8 r = 0, g = 0, b = 0, a = 0;
	};

	// Le pixel du CENTRE. Pas le premier : sur une cible mal orientée ou mal
	// rognée, le coin (0,0) est justement l'endroit le plus susceptible d'être
	// hors de ce qui a été peint.
	Pixel Centre(const uint8 *px) {
		const usize i = ((usize)(kH / 2) * kW + (kW / 2)) * 4u;
		Pixel p;
		p.r = px[i + 0];
		p.g = px[i + 1];
		p.b = px[i + 2];
		p.a = px[i + 3];
		return p;
	}

	// Rend true si la relecture a fonctionné. `out` = pixel central.
	bool EffacerEtRelire(renderer::NkOffscreenTarget &cible, NkIDevice *dev, float32 r, float32 g, float32 b,
						 uint8 *tampon, Pixel &out) {
		NkICommandBuffer *cmd = dev->CreateCommandBuffer();
		if (cmd == nullptr || !cmd->Begin())
			return false;
		cible.BeginCapture(cmd, true, math::NkVec4f(r, g, b, 1.f), true);
		// Aucune géométrie : l'effacement EST la mesure. Ajouter un triangle
		// mêlerait deux questions (le device rend-il ? le pipeline compile-t-il ?)
		// et un échec ne dirait plus laquelle des deux a cédé.
		cible.EndCapture(cmd);
		cmd->End();
		dev->Submit(&cmd, 1);
		dev->WaitIdle();
		for (usize i = 0; i < (usize)kW * kH * 4u; ++i)
			tampon[i] = 0;
		if (!cible.ReadbackPixels(tampon, kW * 4u))
			return false;
		out = Centre(tampon);
		return true;
	}

	void SonderUnDorsal(NkGraphicsApi api) {
		const char *nom = NkGraphicsApiName(api);
		std::printf("\n--- dorsal %s ---\n", nom);
		char d[220];

		// ── m1 : le device, SANS SURFACE ─────────────────────────────────────
		// Exactement le motif de NKTensor (NkTensorGpu.cpp:289-292), qui l'utilise
		// déjà en production pour le compute : un NkDeviceInitInfo dont la surface
		// n'est jamais renseignée.
		NkDeviceInitInfo di;
		di.api = api;
		di.context.software.threading = true;
		NkIDevice *dev = NkDeviceFactory::Create(di);

		const bool m1 = (dev != nullptr) && dev->IsValid();
		std::snprintf(d, sizeof(d), "%s", dev == nullptr ? "Create a rendu nullptr"
									 : dev->IsValid()	 ? "cree et valide, sans surface"
														 : "cree mais IsValid() faux");
		Verdict(nom, "m1 device sans fenetre", m1, d);
		if (!m1) {
			if (dev)
				NkDeviceFactory::Destroy(dev);
			return;
		}

		// ── m2 : la cible hors ecran ─────────────────────────────────────────
		renderer::NkTextureLibrary texlib;
		// NkRendererResult.h:23-27 : tous les codes >= 0 sont des succes (NK_OK,
		// NK_NOT_READY, NK_PARTIAL, NK_FALLBACK_USED). Comparer a NK_OK seul
		// rejetterait un fallback qui a pourtant marche.
		const bool libOk = ((int32)texlib.Init(dev) >= 0);
		if (!libOk) {
			Verdict(nom, "m2 cible hors ecran", false, "NkTextureLibrary::Init refuse");
			NkDeviceFactory::Destroy(dev);
			return;
		}

		renderer::NkOffscreenDesc desc;
		desc.width = kW;
		desc.height = kH;
		desc.hasDepth = true;
		desc.readable = true;
		desc.readback = true; // sans lui, ReadbackPixels rend false d'entree
		desc.name = "SondeSansFenetre";

		renderer::NkOffscreenTarget cible;
		const bool m2 = cible.Init(dev, &texlib, desc) && cible.IsValid();
		std::snprintf(d, sizeof(d), "%ux%u, readback demande", kW, kH);
		Verdict(nom, "m2 cible hors ecran", m2, d);
		if (!m2) {
			texlib.Shutdown();
			NkDeviceFactory::Destroy(dev);
			return;
		}

		// ── m3 et m4 ─────────────────────────────────────────────────────────
		uint8 *tampon = (uint8 *)memory::NkAlloc((usize)kW * kH * 4u);
		if (tampon == nullptr) {
			Verdict(nom, "m3 une passe se relit", false, "allocation du tampon refusee");
			cible.Shutdown();
			texlib.Shutdown();
			NkDeviceFactory::Destroy(dev);
			return;
		}

		Pixel A, B;
		const bool luA = EffacerEtRelire(cible, dev, 0.20f, 0.60f, 0.90f, tampon, A);
		std::snprintf(d, sizeof(d), "effacement A (0,20 0,60 0,90) -> rgba(%u, %u, %u, %u)", (unsigned)A.r,
					  (unsigned)A.g, (unsigned)A.b, (unsigned)A.a);
		Verdict(nom, "m3 une passe se relit", luA, d);

		bool m4 = false;
		if (luA) {
			const bool luB = EffacerEtRelire(cible, dev, 0.90f, 0.20f, 0.20f, tampon, B);
			if (!luB) {
				Verdict(nom, "m4 la couleur relue est la bonne", false, "la seconde relecture a echoue");
			} else {
				// ── L'ATTENDU, ÉCRIT AVANT LA MESURE ─────────────────────────
				// A est bleuté, B est rougi. On n'exige PAS des valeurs absolues :
				// le format de la cible est sRGB, la conversion n'est pas linéaire,
				// et exiger « 0,20 -> 51 » ferait échouer une sonde parfaitement
				// juste. On exige l'ORDRE, que toute conversion monotone préserve :
				//     B.r > A.r   ET   B.g < A.g   ET   B.b < A.b
				// et, surtout, que les deux relectures DIFFERENT.
				const bool different = (A.r != B.r) || (A.g != B.g) || (A.b != B.b);
				const bool ordre = (B.r > A.r) && (B.g < A.g) && (B.b < A.b);
				m4 = different && ordre;
				std::snprintf(d, sizeof(d), "B (0,90 0,20 0,20) -> rgba(%u, %u, %u, %u) | different=%s ordre=%s",
							  (unsigned)B.r, (unsigned)B.g, (unsigned)B.b, (unsigned)B.a, different ? "oui" : "NON",
							  ordre ? "oui" : "NON");
				Verdict(nom, "m4 la couleur relue est la bonne", m4, d);

				// ── LE NEGATIF, dit explicitement ────────────────────────────
				// Si `different` etait faux, ce ne serait PAS un echec du dorsal :
				// ce serait la preuve que la relecture ne depend pas de ce qu'on
				// a demande, donc que l'instrument ment. La distinction merite sa
				// propre ligne, sinon elle se perd dans un rouge parmi d'autres.
				if (!different)
					std::printf("       ⚠️ DEUX EFFACEMENTS OPPOSES, UNE SEULE RELECTURE : c'est\n"
								"          l'instrument qui ment, pas le dorsal. Ne rien batir dessus.\n");
			}
		}

		if (m1 && m2 && luA && m4)
			gDorsauxOk++;

		memory::NkFree(tampon);
		cible.Shutdown();
		texlib.Shutdown();
		NkDeviceFactory::Destroy(dev);
	}

} // namespace

int main() {
#if defined(_WIN32)
	SetConsoleTitleA("*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***");
#endif
	std::printf("=============================================================================\n");
	std::printf(" *** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***\n");
	std::printf(" NkOffscreenProbe — un peripherique RHI se cree-t-il SANS FENETRE,\n");
	std::printf(" et la couleur relue est-elle celle qu'on a demandee ?\n");
	std::printf("-----------------------------------------------------------------------------\n");
	std::printf(" m4 est le seul critere qui compte : DEUX effacements differents doivent\n");
	std::printf(" donner DEUX relectures differentes. Une image non noire ne prouve rien —\n");
	std::printf(" NkOffscreenTarget::Init efface deja la cible a la creation.\n");
	std::printf(" Sur OpenGL SEULEMENT, NKRHI ouvre lui-meme une fenetre CACHEE 1x1\n");
	std::printf(" (NkOpenglDevice.cpp:388-399) : GL exige une surface. Ce n'est pas moi qui\n");
	std::printf(" l'ouvre, et je ne la ferme pas a la main — mais je ne le cache pas.\n");
	std::printf("=============================================================================\n");

	const NkGraphicsApi ordre[4] = {NkGraphicsApi::NK_GFX_API_OPENGL, NkGraphicsApi::NK_GFX_API_VULKAN,
									NkGraphicsApi::NK_GFX_API_DX11, NkGraphicsApi::NK_GFX_API_DX12};
	for (int i = 0; i < 4; ++i)
		SonderUnDorsal(ordre[i]);

	std::printf("\n-----------------------------------------------------------------------------\n");
	std::printf(" BILAN : %d verts, %d rouges — %d dorsal(aux) sur 4 rendent SANS FENETRE\n", gPass, gFail,
				gDorsauxOk);
	std::printf(" Status: %s\n", (gDorsauxOk > 0) ? "SUCCESS" : "FAILURE");
	if (gDorsauxOk == 0)
		std::printf(" -> AUCUN dorsal ne rend sans fenetre. Ce n'est pas un chantier du\n"
					"    sequenceur : c'est un chantier de NKRHI.\n");
	std::printf("-----------------------------------------------------------------------------\n");
	return (gDorsauxOk > 0) ? 0 : 1;
}
