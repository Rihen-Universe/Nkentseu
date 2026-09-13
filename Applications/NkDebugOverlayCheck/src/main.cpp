// =============================================================================
// Applications/NkDebugOverlayCheck/src/main.cpp
// =============================================================================
// C1 : `cfg.debugOverlay` A-T-IL UN EFFET ? (et non : laisse-t-il une trace ?)
// -----------------------------------------------------------------------------
// Le champ etait declare, documente, et n avait AUCUNE lecture dans le depot.
// Rodolf a tranche le 27/08, lecture (A) sur trois proposees : debugOverlay
// IMPLIQUE le sous-systeme d overlay.
//
// ⚠️ POURQUOI CE BANC MESURE UN AVANT/APRES ET PAS UNE LIGNE DE JOURNAL
// -----------------------------------------------------------------------------
// En cablant C1, j ai failli ecrire un simple avertissement :
//     logger.Warn("debugOverlay demande sans NK_SS_OVERLAY : sans effet");
// C etait raisonnable, et c aurait ete un piege. Le critere du detecteur D2 est
// « ce champ est-il LU ? », et journaliser EST une lecture. Le champ serait sorti
// de la liste des candidats, la dette aurait disparu du compteur, ET LE DRAPEAU
// N AURAIT TOUJOURS RIEN FAIT.
//
//   « Le drapeau doit avoir un EFFET, pas une trace. »
//
// Ce banc ne regarde donc aucun message. Il regarde ce que le renderer ALLOUE.
//
// CE QU IL MESURE
// -----------------------------------------------------------------------------
// Dans les deux cas, NK_SS_OVERLAY est ABSENT des sous-systemes demandes :
//     debugOverlay = false  ->  GetOverlay() == nullptr    LE TEMOIN
//     debugOverlay = true   ->  GetOverlay() != nullptr    LA MESURE
//
// LE TEMOIN PASSE AVANT, ET IL EST INDISPENSABLE ICI
// -----------------------------------------------------------------------------
// Si l overlay etait alloue DE TOUTE FACON -- par une dependance interne, par un
// autre sous-systeme -- le second cas rendrait « non nul » sans que le drapeau y
// soit pour rien, et ce banc crierait victoire sur un montage qui ne prouve rien.
// Le temoin etablit que SANS le drapeau, il n y a PAS d overlay. Sans lui, la
// mesure ne mesure rien. Temoin en echec -> IGNORE, jamais OK, jamais ECHEC.
//
// IGNORE EST UN TROISIEME ETAT (code 77) : ce banc exige un peripherique GPU.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRenderer/NkRenderer.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	const int kIgnore = 77;
	int32 gPass = 0;
	int32 gFail = 0;

	void Verifier(bool condition, const char *libelle) {
		if (condition) {
			++gPass;
			printf("  [OK]   %s\n", libelle);
		} else {
			++gFail;
			printf("  [FAIL] %s\n", libelle);
		}
	}

	NkIDevice *Ouvrir(NkGraphicsApi api) {
		NkDeviceInitInfo di;
		di.api = api;
		di.width = 0;
		di.height = 0;
		di.context.software.threading = true;
		NkIDevice *dev = NkDeviceFactory::Create(di);
		if (dev == nullptr)
			return nullptr;
		if (!dev->IsValid()) {
			NkDeviceFactory::Destroy(dev);
			return nullptr;
		}
		return dev;
	}

	// UNE SEULE fabrique : les deux cas ne different QUE par le drapeau mesure.
	// ⚠️ NK_SS_OVERLAY est deliberement ABSENT : c est tout l interet. Si on le
	// demandait, l overlay serait alloue dans les deux cas et le banc ne
	// mesurerait rien du drapeau.
	NkRendererConfig Decrire(NkIDevice *dev, bool overlayDemande) {
		NkRendererConfig c;
		c.api = dev->GetApi();
		c.width = 256;
		c.height = 256;
		c.subsystems = NK_SS_RENDER2D | NK_SS_TEXT; // de quoi construire un overlay, sans le demander
		c.debugOverlay = overlayDemande;
		return c;
	}

	// 1 = overlay alloue, 0 = absent, -1 = le renderer n a pas pu etre cree.
	int32 OverlayApres(NkIDevice *dev, bool overlayDemande) {
		NkRendererConfig cfg = Decrire(dev, overlayDemande);
		NkRenderer *r = NkRenderer::Create(dev, cfg);
		if (r == nullptr)
			return -1;
		const int32 res = (r->GetOverlay() != nullptr) ? 1 : 0;
		NkRenderer::Destroy(r);
		return res;
	}

} // namespace

int main() {
	printf("=== NkDebugOverlayCheck -- cfg.debugOverlay a-t-il un EFFET ? ===\n");
	printf("Il ne regarde AUCUN message : il regarde ce que le renderer ALLOUE.\n");
	fflush(stdout);

	const NkGraphicsApi essais[] = {NkGraphicsApi::NK_GFX_API_DX11, NkGraphicsApi::NK_GFX_API_DX12,
									NkGraphicsApi::NK_GFX_API_OPENGL, NkGraphicsApi::NK_GFX_API_VULKAN};
	NkIDevice *dev = nullptr;
	for (uint32 i = 0; i < sizeof(essais) / sizeof(essais[0]); ++i) {
		printf("  ... tentative %s\n", NkGraphicsApiName(essais[i]));
		fflush(stdout);
		dev = Ouvrir(essais[i]);
		if (dev != nullptr)
			break;
	}
	if (dev == nullptr) {
		printf("\n[IGNORE] aucun peripherique headless n a pu etre cree sur cette machine.\n");
		printf("         Ce banc n a RIEN mesure. Code %d : ni OK, ni ECHEC.\n", kIgnore);
		return kIgnore;
	}
	printf("\n--- peripherique : %s (LU sur le device) ---\n", NkGraphicsApiName(dev->GetApi()));

	// ---- LE TEMOIN : sans le drapeau, PAS d overlay --------------------------
	printf("\n-- temoin : NK_SS_OVERLAY absent ET debugOverlay = FALSE --\n");
	const int32 sans = OverlayApres(dev, false);
	if (sans < 0) {
		printf("\n[IGNORE] le renderer n a pas pu etre cree du tout. Code %d.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	if (sans != 0) {
		printf("\n[IGNORE] l overlay est alloue MEME SANS le demander.\n");
		printf("         Le cas mesure rendrait « non nul » sans que le drapeau y soit\n");
		printf("         pour rien, et ce banc crierait victoire sur un montage qui ne\n");
		printf("         prouve rien. A REGARDER : une dependance interne alloue l overlay.\n");
		printf("         Ce banc n est pas celui qui peut dire laquelle. Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("  [TEMOIN] pas d overlay. Le drapeau est mesurable.\n");

	// ---- LA MESURE -----------------------------------------------------------
	printf("\n-- mesure : NK_SS_OVERLAY absent ET debugOverlay = TRUE --\n");
	const int32 avec = OverlayApres(dev, true);
	if (avec < 0) {
		printf("\n[IGNORE] le renderer n a pas pu etre cree avec debugOverlay = true.\n");
		printf("         Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}

	Verifier(avec == 1, "cfg.debugOverlay = true  -> NkOverlayRenderer ALLOUE (l effet)");
	Verifier(sans == 0, "cfg.debugOverlay = false -> pas d overlay (le temoin)");

	NkDeviceFactory::Destroy(dev);

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	if (gFail != 0) {
		printf("\nLe drapeau est declare et n a pas d effet : le poser a true ne change rien\n");
		printf("a ce que le renderer alloue. C est la capacite creuse d origine, intacte.\n");
	} else {
		printf("\n[LIMITE] dite plutot que tue : ce banc mesure l ALLOCATION de l overlay,\n");
		printf("   pas ce qui est DESSINE. Le panneau de statistiques appartient a\n");
		printf("   l application -- DrawStats est appele par 18 sites applicatifs et le\n");
		printf("   renderer ne le dessine jamais lui-meme. C est la lecture (A) qui a ete\n");
		printf("   tranchee, et (B) « le renderer dessine » a ete ecartee : elle aurait\n");
		printf("   donne deux panneaux superposes chez NK3DModeler.\n");
	}
	return gFail == 0 ? 0 : 1;
}
