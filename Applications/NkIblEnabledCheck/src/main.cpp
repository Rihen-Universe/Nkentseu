// =============================================================================
// Applications/NkIblEnabledCheck/src/main.cpp
// =============================================================================
// B4 : LE DRAPEAU QUE PERSONNE NE LISAIT, ET LE BANC SANS LEQUEL ON NE SAURAIT PAS
// -----------------------------------------------------------------------------
// NkIBLConfig::enabled etait declare, documente, et n avait AUCUNE lecture dans
// tout le depot. Les ONZE autres champs de NkIBLConfig sont lus par
// NkRendererImpl::InitEnvironment() ; lui seul ne l etait pas. Regler
// cfg.ibl.enabled = false ne faisait RIEN.
//
// POURQUOI CE BANC EXISTE, ET POURQUOI IL EST NE EN MEME TEMPS QUE LE CORRECTIF
// -----------------------------------------------------------------------------
// Le correctif est UN `if`. Mais la mesure du 27/08 disait aussi : ZERO ecriture
// de `ibl.enabled` dans tout l arbre. Personne ne le met a false aujourd hui.
//
//   « Implementer une capacite que rien n exerce, c est ajouter un chemin mort
//     en croyant regler une dette. »
//
// Un `if` que personne n emprunte est du code dont on ne sait pas s il est juste
// -- et il aurait rejoint la liste des chemins jamais parcourus que ce chantier
// passe son temps a debusquer. LE `if` SANS LE BANC SERAIT UNE REGRESSION
// DEGUISEE EN PROGRES. Les deux, ou aucun.
//
// CE QU IL MESURE
// -----------------------------------------------------------------------------
//   enabled = true   -> GetEnvironment() != nullptr     (le TEMOIN)
//   enabled = false  -> GetEnvironment() == nullptr     (LA MESURE)
//
// LE TEMOIN PASSE AVANT LA MESURE, ET C EST LA REGLE DE CE CHANTIER
// -----------------------------------------------------------------------------
// Si l IBL ne se construit pas MEME quand on le demande, alors le second cas
// rendra nullptr pour une raison qui n a rien a voir avec le drapeau, et le banc
// crierait victoire sur un montage casse. « Avant de conclure que le controle
// voit, prouver que ce qu on lui a montre etait visible. » Temoin en echec ->
// IGNORE, jamais OK, jamais ECHEC.
//
// IGNORE EST UN TROISIEME ETAT (code 77) : ce banc exige un peripherique GPU.
// Sur une machine sans carte il n a RIEN mesure, et le compter vert ferait croire
// la question reglee.
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
		di.api = api; // pas de di.surface -> headless
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

	// UNE SEULE fabrique de config : les deux cas ne different QUE par le drapeau
	// mesure. Sinon le temoin ne temoigne de rien.
	NkRendererConfig Decrire(NkIDevice *dev, bool iblActive) {
		NkRendererConfig c;
		c.api = dev->GetApi();
		c.width = 256;
		c.height = 256;
		c.subsystems = NK_SS_RENDER3D; // c est ce drapeau qui declenche InitEnvironment
		c.ibl.enabled = iblActive;
		return c;
	}

	// Rend 1 si l environnement existe, 0 s il est nul, -1 si le renderer n a pas
	// pu etre cree du tout (on ne conclut alors RIEN).
	int32 EnvironnementApres(NkIDevice *dev, bool iblActive) {
		NkRendererConfig cfg = Decrire(dev, iblActive);
		NkRenderer *r = NkRenderer::Create(dev, cfg);
		if (r == nullptr)
			return -1;
		const int32 res = (r->GetEnvironment() != nullptr) ? 1 : 0;
		NkRenderer::Destroy(r);
		return res;
	}

} // namespace

int main() {
	printf("=== NkIblEnabledCheck -- cfg.ibl.enabled est-il HONORE ? ===\n");
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
		printf("         Ce banc n a RIEN mesure. Ce n est pas un succes.\n");
		printf("         Code %d : ni OK, ni ECHEC.\n", kIgnore);
		return kIgnore;
	}

	// L API est LUE sur le device, jamais supposee -- meme correctif que les deux
	// bancs MSAA le 27/08 : un banc qui annonce son backend sans le lire est vrai
	// jusqu au jour ou il ne l est plus.
	printf("\n--- peripherique : %s (LU sur le device) ---\n", NkGraphicsApiName(dev->GetApi()));

	// ---- LE TEMOIN, AVANT TOUTE MESURE ---------------------------------------
	printf("\n-- temoin : avec ibl.enabled = TRUE, l environnement doit EXISTER --\n");
	const int32 avec = EnvironnementApres(dev, true);
	if (avec < 0) {
		printf("\n[IGNORE] le renderer n a pas pu etre cree du tout (ibl.enabled = true).\n");
		printf("         Le drapeau n y est pour rien : la question ne peut pas etre posee\n");
		printf("         sur ce montage. Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	if (avec != 1) {
		printf("\n[IGNORE] avec ibl.enabled = TRUE, GetEnvironment() rend deja nullptr.\n");
		printf("         Le cas mesure rendrait nullptr lui aussi, et ce banc crierait\n");
		printf("         victoire sur un montage casse : le drapeau n aurait rien prouve.\n");
		printf("         A REGARDER : l IBL ne se construit pas meme quand on la demande.\n");
		printf("         Ce banc n est pas celui qui peut dire pourquoi. Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("  [TEMOIN] environnement present. Le drapeau est mesurable.\n");

	// ---- LA MESURE -----------------------------------------------------------
	printf("\n-- mesure : avec ibl.enabled = FALSE, l environnement doit etre ABSENT --\n");
	const int32 sans = EnvironnementApres(dev, false);
	if (sans < 0) {
		printf("\n[IGNORE] le renderer n a pas pu etre cree avec ibl.enabled = false.\n");
		printf("         Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}

	Verifier(sans == 0, "cfg.ibl.enabled = false -> NkEnvironmentSystem NON alloue");
	Verifier(avec == 1, "cfg.ibl.enabled = true  -> NkEnvironmentSystem alloue");

	NkDeviceFactory::Destroy(dev);

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	if (gFail != 0) {
		printf("\nLe drapeau est declare et n est pas honore : le regler a false ne change\n");
		printf("rien, et l IBL se construit quand meme -- convolutions et cubemaps compris.\n");
	} else {
		printf("\n[LIMITE] dite plutot que tue : ce banc mesure l ALLOCATION du sous-systeme,\n");
		printf("   pas l absence de contribution IBL dans l image. Un rendu qui refleterait\n");
		printf("   encore un environnement sans l avoir alloue passerait ici -- il faudrait\n");
		printf("   une mesure d image pour le dire, et ce banc ne la fait pas.\n");
	}
	return gFail == 0 ? 0 : 1;
}
